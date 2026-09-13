// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "sdb/datasource/gdal/ogr_feature_kind.h"

#include "geometry.h"
#include "matrix2d.h"
#include "style.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cstring>

namespace sdb {
namespace datasource {

namespace {

OGRPoint* first_point(OGRGeometry* geom) {
  if (!geom) {
    return nullptr;
  }
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (wt == wkbPoint) {
    return geom->toPoint();
  }
  if (wt == wkbMultiPoint) {
    auto* multi = geom->toMultiPoint();
    if (multi && multi->getNumGeometries() > 0) {
      return multi->getGeometryRef(0)->toPoint();
    }
  }
  return nullptr;
}

OGRLineString* first_linestring(OGRGeometry* geom) {
  if (!geom) {
    return nullptr;
  }
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (wt == wkbLineString) {
    return geom->toLineString();
  }
  if (wt == wkbMultiLineString) {
    auto* multi = geom->toMultiLineString();
    if (multi && multi->getNumGeometries() > 0) {
      return multi->getGeometryRef(0)->toLineString();
    }
  }
  return nullptr;
}

OGRPolygon* first_polygon(OGRGeometry* geom) {
  if (!geom) {
    return nullptr;
  }
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (wt == wkbPolygon) {
    return geom->toPolygon();
  }
  if (wt == wkbMultiPolygon) {
    auto* multi = geom->toMultiPolygon();
    if (multi && multi->getNumGeometries() > 0) {
      return multi->getGeometryRef(0)->toPolygon();
    }
  }
  return nullptr;
}

bool encode_point(const Smt_Geo::SmtGeometry* src, OGRFeature* dst) {
  const auto* pt = dynamic_cast<const Smt_Geo::SmtPoint*>(src);
  if (!pt) {
    return false;
  }
  OGRPoint ogr_pt(pt->GetX(), pt->GetY());
  return dst->SetGeometry(&ogr_pt) == OGRERR_NONE;
}

Smt_Geo::SmtGeometry* decode_point(OGRFeature* src) {
  OGRPoint* po_point = first_point(src->GetGeometryRef());
  if (!po_point) {
    return nullptr;
  }
  return new Smt_Geo::SmtPoint(po_point->getX(), po_point->getY());
}

bool encode_linestring(const Smt_Geo::SmtGeometry* src, OGRFeature* dst) {
  const auto* line = dynamic_cast<const Smt_Geo::SmtLineString*>(src);
  if (!line) {
    return false;
  }
  OGRLineString ogr_line;
  const int n = line->GetNumPoints();
  ogr_line.setNumPoints(n);
  for (int i = 0; i < n; ++i) {
    ogr_line.setPoint(i, line->GetX(i), line->GetY(i));
  }
  return dst->SetGeometry(&ogr_line) == OGRERR_NONE;
}

Smt_Geo::SmtGeometry* decode_linestring(OGRFeature* src) {
  OGRLineString* po_line = first_linestring(src->GetGeometryRef());
  if (!po_line) {
    return nullptr;
  }
  auto* line = new Smt_Geo::SmtLineString();
  const int n = po_line->getNumPoints();
  line->SetNumPoints(n);
  for (int i = 0; i < n; ++i) {
    line->SetPoint(i, po_line->getX(i), po_line->getY(i));
  }
  return line;
}

bool encode_polygon(const Smt_Geo::SmtGeometry* src, OGRFeature* dst) {
  const auto* poly = dynamic_cast<const Smt_Geo::SmtPolygon*>(src);
  if (!poly) {
    return false;
  }
  const Smt_Geo::SmtLinearRing* ring = poly->GetExteriorRing();
  if (!ring) {
    return false;
  }
  OGRLinearRing ogr_ring;
  const int n = ring->GetNumPoints();
  ogr_ring.setNumPoints(n);
  for (int i = 0; i < n; ++i) {
    ogr_ring.setPoint(i, ring->GetX(i), ring->GetY(i));
  }
  ogr_ring.closeRings();
  OGRPolygon ogr_poly;
  ogr_poly.addRing(&ogr_ring);
  return dst->SetGeometry(&ogr_poly) == OGRERR_NONE;
}

Smt_Geo::SmtGeometry* decode_polygon(OGRFeature* src) {
  OGRPolygon* po_poly = first_polygon(src->GetGeometryRef());
  if (!po_poly) {
    return nullptr;
  }
  OGRLinearRing* po_ring = po_poly->getExteriorRing();
  if (!po_ring) {
    return nullptr;
  }
  auto* ring = new Smt_Geo::SmtLinearRing();
  const int n = po_ring->getNumPoints();
  ring->SetNumPoints(n);
  for (int i = 0; i < n; ++i) {
    ring->SetPoint(i, po_ring->getX(i), po_ring->getY(i));
  }
  ring->CloseRings();
  auto* poly = new Smt_Geo::SmtPolygon();
  poly->AddRingDirectly(ring);
  return poly;
}

}  // namespace

Smt_GIS::SmtFeatureType infer_feature_type(OGRFeature* src,
                                           Smt_GIS::SmtFeatureType hint) {
  if (hint == Smt_GIS::SmtFtTin || hint == Smt_GIS::SmtFtGrid ||
      hint == Smt_GIS::SmtFtAnno || hint == Smt_GIS::SmtFtDot ||
      hint == Smt_GIS::SmtFtCurve || hint == Smt_GIS::SmtFtSurface) {
    return hint;
  }
  if (!src) {
    return Smt_GIS::SmtFtUnknown;
  }
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom) {
    return Smt_GIS::SmtFtUnknown;
  }
  switch (wkbFlatten(geom->getGeometryType())) {
    case wkbPoint:
      return src->GetFieldIndex("anno") >= 0 ? Smt_GIS::SmtFtAnno
                                            : Smt_GIS::SmtFtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return Smt_GIS::SmtFtCurve;
    case wkbPolygon:
    case wkbMultiPolygon:
      return Smt_GIS::SmtFtSurface;
    case wkbTIN:
      return Smt_GIS::SmtFtTin;
    case wkbMultiPoint:
      return src->GetFieldIndex("grid_row") >= 0 ? Smt_GIS::SmtFtGrid
                                                : Smt_GIS::SmtFtDot;
    default:
      return Smt_GIS::SmtFtUnknown;
  }
}

Smt_GIS::SmtFeatureType feature_type_of(OGRLayer* layer) {
  if (!layer) {
    return Smt_GIS::SmtFtUnknown;
  }
  const OGRwkbGeometryType wkb = wkbFlatten(layer->GetGeomType());
  if (wkb == wkbPoint && layer->FindFieldIndex("anno", TRUE) >= 0) {
    return Smt_GIS::SmtFtAnno;
  }
  if (wkb == wkbMultiPoint && layer->FindFieldIndex("grid_row", TRUE) >= 0) {
    return Smt_GIS::SmtFtGrid;
  }
  switch (wkb) {
    case wkbPoint:
      return Smt_GIS::SmtFtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return Smt_GIS::SmtFtCurve;
    case wkbPolygon:
      return layer->FindFieldIndex("area", TRUE) >= 0 ? Smt_GIS::SmtFtSurface
                                                     : Smt_GIS::SmtFtTin;
    case wkbMultiPolygon:
    case wkbTIN:
      return Smt_GIS::SmtFtTin;
    default:
      return Smt_GIS::SmtFtUnknown;
  }
}

void copy_smt_style_to_ogr(const Smt_Base::SmtStyle* src, OGRFeature* dst) {
  if (!src || !dst) {
    return;
  }
  const int oi = dst->GetFieldIndex("style");
  if (oi < 0) {
    return;
  }
  dst->SetField(oi, static_cast<int>(sizeof(Smt_Base::SmtStyle)),
                reinterpret_cast<const void*>(src));
}

Smt_Base::SmtStyle* copy_ogr_style_from_ogr(OGRFeature* src) {
  if (!src) {
    return nullptr;
  }
  const int oi = src->GetFieldIndex("style");
  if (oi < 0) {
    return nullptr;
  }
  int nbytes = 0;
  GByte* data = src->GetFieldAsBinary(oi, &nbytes);
  if (!data || nbytes < static_cast<int>(sizeof(Smt_Base::SmtStyle))) {
    return nullptr;
  }
  auto* style = new Smt_Base::SmtStyle();
  std::memcpy(style, data, sizeof(Smt_Base::SmtStyle));
  return style;
}

bool feature_kind_traits<Smt_GIS::SmtFtDot>::encode_geom(
    const Smt_Geo::SmtGeometry* src, OGRFeature* dst) {
  return encode_point(src, dst);
}

Smt_Geo::SmtGeometry* feature_kind_traits<Smt_GIS::SmtFtDot>::decode_geom(
    OGRFeature* src) {
  return decode_point(src);
}

bool feature_kind_traits<Smt_GIS::SmtFtCurve>::encode_geom(
    const Smt_Geo::SmtGeometry* src, OGRFeature* dst) {
  return encode_linestring(src, dst);
}

Smt_Geo::SmtGeometry* feature_kind_traits<Smt_GIS::SmtFtCurve>::decode_geom(
    OGRFeature* src) {
  return decode_linestring(src);
}

bool feature_kind_traits<Smt_GIS::SmtFtSurface>::encode_geom(
    const Smt_Geo::SmtGeometry* src, OGRFeature* dst) {
  return encode_polygon(src, dst);
}

Smt_Geo::SmtGeometry* feature_kind_traits<Smt_GIS::SmtFtSurface>::decode_geom(
    OGRFeature* src) {
  return decode_polygon(src);
}

bool feature_kind_traits<Smt_GIS::SmtFtAnno>::encode_geom(
    const Smt_Geo::SmtGeometry* src, OGRFeature* dst) {
  return encode_point(src, dst);
}

Smt_Geo::SmtGeometry* feature_kind_traits<Smt_GIS::SmtFtAnno>::decode_geom(
    OGRFeature* src) {
  return decode_point(src);
}

bool feature_kind_traits<Smt_GIS::SmtFtTin>::encode_geom(
    const Smt_Geo::SmtGeometry* src, OGRFeature* dst) {
  const auto* tin = dynamic_cast<const Smt_Geo::SmtTin*>(src);
  if (!tin) {
    return false;
  }
  OGRMultiPolygon multi;
  const int ntri = tin->GetTriangleCount();
  for (int t = 0; t < ntri; ++t) {
    const Smt_Core::SmtTriangle tri = tin->GetTriangle(t);
    const Smt_Geo::SmtPoint pa = tin->GetPoint(static_cast<int>(tri.a));
    const Smt_Geo::SmtPoint pb = tin->GetPoint(static_cast<int>(tri.b));
    const Smt_Geo::SmtPoint pc = tin->GetPoint(static_cast<int>(tri.c));
    OGRLinearRing ring;
    ring.setNumPoints(4);
    ring.setPoint(0, pa.GetX(), pa.GetY());
    ring.setPoint(1, pb.GetX(), pb.GetY());
    ring.setPoint(2, pc.GetX(), pc.GetY());
    ring.setPoint(3, pa.GetX(), pa.GetY());
    ring.closeRings();
    OGRPolygon poly;
    poly.addRing(&ring);
    multi.addGeometry(&poly);
  }
  return dst->SetGeometry(&multi) == OGRERR_NONE;
}

Smt_Geo::SmtGeometry* feature_kind_traits<Smt_GIS::SmtFtTin>::decode_geom(
    OGRFeature* src) {
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom) {
    return nullptr;
  }
  auto add_triangle_from_ring = [](Smt_Geo::SmtTin* tin, OGRLinearRing* ring) {
    if (!ring || ring->getNumPoints() < 3) {
      return;
    }
    Smt_Geo::SmtPoint p0(ring->getX(0), ring->getY(0));
    Smt_Geo::SmtPoint p1(ring->getX(1), ring->getY(1));
    Smt_Geo::SmtPoint p2(ring->getX(2), ring->getY(2));
    tin->AddPoint(&p0);
    const int ia = tin->GetPointCount() - 1;
    tin->AddPoint(&p1);
    const int ib = tin->GetPointCount() - 1;
    tin->AddPoint(&p2);
    const int ic = tin->GetPointCount() - 1;
    Smt_Core::SmtTriangle tri;
    tri.a = ia;
    tri.b = ib;
    tri.c = ic;
    tin->AddTriangle(&tri);
  };

  auto* tin = new Smt_Geo::SmtTin();
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (wt == wkbMultiPolygon) {
    auto* multi = geom->toMultiPolygon();
    const int n = multi->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      OGRPolygon* poly = multi->getGeometryRef(i)->toPolygon();
      add_triangle_from_ring(tin, poly ? poly->getExteriorRing() : nullptr);
    }
  } else if (wt == wkbPolygon) {
    add_triangle_from_ring(tin, geom->toPolygon()->getExteriorRing());
  } else if (wt == wkbTIN) {
    auto* coll = geom->toGeometryCollection();
    const int n = coll ? coll->getNumGeometries() : 0;
    for (int i = 0; i < n && coll; ++i) {
      OGRGeometry* part = coll->getGeometryRef(i);
      if (part && wkbFlatten(part->getGeometryType()) == wkbPolygon) {
        add_triangle_from_ring(tin, part->toPolygon()->getExteriorRing());
      } else if (part && wkbFlatten(part->getGeometryType()) == wkbTriangle) {
        add_triangle_from_ring(tin, part->toPolygon()->getExteriorRing());
      }
    }
  } else {
    delete tin;
    return nullptr;
  }
  if (tin->GetTriangleCount() < 1 && tin->GetPointCount() < 3) {
    delete tin;
    return nullptr;
  }
  return tin;
}

bool feature_kind_traits<Smt_GIS::SmtFtGrid>::encode_geom(
    const Smt_Geo::SmtGeometry* src, OGRFeature* dst) {
  const auto* grid = dynamic_cast<const Smt_Geo::SmtGrid*>(src);
  if (!grid || !grid->GetGridNodeBuf()) {
    return false;
  }
  int rows = 0;
  int cols = 0;
  grid->GetSize(rows, cols);
  const Matrix2D<Smt_Geo::RawPoint>* buf = grid->GetGridNodeBuf();
  OGRMultiPoint mp;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const Smt_Geo::RawPoint& pt = buf->GetElement(r, c);
      OGRPoint ogr_pt(pt.x, pt.y);
      mp.addGeometry(&ogr_pt);
    }
  }
  return dst->SetGeometry(&mp) == OGRERR_NONE;
}

Smt_Geo::SmtGeometry* feature_kind_traits<Smt_GIS::SmtFtGrid>::decode_geom(
    OGRFeature* src) {
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom || wkbFlatten(geom->getGeometryType()) != wkbMultiPoint) {
    return nullptr;
  }
  int rows = 0;
  int cols = 0;
  const int ri = src->GetFieldIndex("grid_row");
  const int ci = src->GetFieldIndex("grid_col");
  if (ri >= 0) {
    rows = src->GetFieldAsInteger(ri);
  }
  if (ci >= 0) {
    cols = src->GetFieldAsInteger(ci);
  }
  auto* mp = geom->toMultiPoint();
  const int n = mp->getNumGeometries();
  if (rows <= 0 || cols <= 0) {
    cols = n;
    rows = 1;
  }
  auto* grid = new Smt_Geo::SmtGrid(rows, cols);
  Matrix2D<Smt_Geo::RawPoint>* buf = grid->GetGridNodeBuf();
  int k = 0;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols && k < n; ++c, ++k) {
      OGRPoint* pt = mp->getGeometryRef(k)->toPoint();
      Smt_Geo::RawPoint raw;
      raw.x = pt->getX();
      raw.y = pt->getY();
      buf->SetElement(raw, r, c);
    }
  }
  return grid;
}

bool feature_kind_traits<Smt_GIS::SmtFtChildImage>::encode_geom(
    const Smt_Geo::SmtGeometry* /*src*/, OGRFeature* /*dst*/) {
  return false;
}

Smt_Geo::SmtGeometry* feature_kind_traits<Smt_GIS::SmtFtChildImage>::decode_geom(
    OGRFeature* /*src*/) {
  return nullptr;
}

bool encode_smt_geometry(const Smt_Geo::SmtGeometry* src, OGRFeature* dst,
                         Smt_GIS::SmtFeatureType ft) {
  if (!src || !dst) {
    return false;
  }
  return visit_feature_kind(ft, [&](auto traits) {
    using Traits = decltype(traits);
    return Traits::encode_geom(src, dst);
  });
}

Smt_Geo::SmtGeometry* decode_ogr_geometry(OGRFeature* src,
                                          Smt_GIS::SmtFeatureType hint) {
  if (!src) {
    return nullptr;
  }
  const Smt_GIS::SmtFeatureType ft = infer_feature_type(src, hint);
  Smt_Geo::SmtGeometry* out = nullptr;
  visit_feature_kind(ft, [&](auto traits) {
    using Traits = decltype(traits);
    out = Traits::decode_geom(src);
    return out != nullptr;
  });
  return out;
}

bool create_vector_layer(GDALDataset* ds, const char* name,
                         Smt_GIS::SmtFeatureType ft, OGRLayer** out) {
  if (!ds || !name || !out) {
    return false;
  }
  *out = nullptr;
  return visit_feature_kind(ft, [&](auto traits) {
    using Traits = decltype(traits);
    if (Traits::is_raster) {
      return false;
    }
    OGRLayer* lyr = ds->CreateLayer(name, nullptr, Traits::wkb, nullptr);
    if (!lyr) {
      return false;
    }
    OGRFieldDefn style("style", OFTBinary);
    lyr->CreateField(&style);
    for_each_extra_field<typename Traits::extra_fields>([&](auto field) {
      OGRFieldDefn defn(field.name, field.ogr_type);
      lyr->CreateField(&defn);
    });
    *out = lyr;
    return true;
  });
}

OGRLayer* create_scratch_layer(GDALDataset* ds, const char* name) {
  if (!ds || !name) {
    return nullptr;
  }
  OGRLayer* lyr = ds->CreateLayer(name, nullptr, wkbUnknown, nullptr);
  if (!lyr) {
    return nullptr;
  }
  OGRFieldDefn style("style", OFTBinary);
  lyr->CreateField(&style);
  return lyr;
}

}  // namespace datasource
}  // namespace sdb
