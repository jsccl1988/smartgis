// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "sdb/datasource/gdal/ogr_feature_kind.h"

#include "algorithm/geo/geometry.h"
#include "base/core/matrix2d.h"
#include "base/style/style.h"

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

bool encode_point(const OGRGeometry* src, OGRFeature* dst) {
  OGRPoint* pt = first_point(const_cast<OGRGeometry*>(src));
  if (!pt) {
    return false;
  }
  return dst->SetGeometry(pt) == OGRERR_NONE;
}

OGRGeometry* decode_point(OGRFeature* src) {
  OGRPoint* po_point = first_point(src->GetGeometryRef());
  return po_point ? po_point->clone() : nullptr;
}

bool encode_linestring(const OGRGeometry* src, OGRFeature* dst) {
  OGRLineString* line = first_linestring(const_cast<OGRGeometry*>(src));
  if (!line) {
    return false;
  }
  return dst->SetGeometry(line) == OGRERR_NONE;
}

OGRGeometry* decode_linestring(OGRFeature* src) {
  OGRLineString* po_line = first_linestring(src->GetGeometryRef());
  return po_line ? po_line->clone() : nullptr;
}

bool encode_polygon(const OGRGeometry* src, OGRFeature* dst) {
  OGRPolygon* poly = first_polygon(const_cast<OGRGeometry*>(src));
  if (!poly) {
    return false;
  }
  return dst->SetGeometry(poly) == OGRERR_NONE;
}

OGRGeometry* decode_polygon(OGRFeature* src) {
  OGRPolygon* po_poly = first_polygon(src->GetGeometryRef());
  return po_poly ? po_poly->clone() : nullptr;
}

}  // namespace

sdb::SmtFeatureType infer_feature_type(OGRFeature* src,
                                           sdb::SmtFeatureType hint) {
  if (hint == sdb::SmtFtTin || hint == sdb::SmtFtGrid ||
      hint == sdb::SmtFtAnno || hint == sdb::SmtFtDot ||
      hint == sdb::SmtFtCurve || hint == sdb::SmtFtSurface) {
    return hint;
  }
  if (!src) {
    return sdb::SmtFtUnknown;
  }
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom) {
    return sdb::SmtFtUnknown;
  }
  switch (wkbFlatten(geom->getGeometryType())) {
    case wkbPoint:
      return src->GetFieldIndex("anno") >= 0 ? sdb::SmtFtAnno
                                            : sdb::SmtFtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return sdb::SmtFtCurve;
    case wkbPolygon:
    case wkbMultiPolygon:
      return sdb::SmtFtSurface;
    case wkbTIN:
      return sdb::SmtFtTin;
    case wkbMultiPoint:
      return src->GetFieldIndex("grid_row") >= 0 ? sdb::SmtFtGrid
                                                : sdb::SmtFtDot;
    default:
      return sdb::SmtFtUnknown;
  }
}

sdb::SmtFeatureType feature_type_of(OGRLayer* layer) {
  if (!layer) {
    return sdb::SmtFtUnknown;
  }
  const OGRwkbGeometryType wkb = wkbFlatten(layer->GetGeomType());
  if (wkb == wkbPoint && layer->FindFieldIndex("anno", TRUE) >= 0) {
    return sdb::SmtFtAnno;
  }
  if (wkb == wkbMultiPoint && layer->FindFieldIndex("grid_row", TRUE) >= 0) {
    return sdb::SmtFtGrid;
  }
  switch (wkb) {
    case wkbPoint:
      return sdb::SmtFtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return sdb::SmtFtCurve;
    case wkbPolygon:
      return layer->FindFieldIndex("area", TRUE) >= 0 ? sdb::SmtFtSurface
                                                     : sdb::SmtFtTin;
    case wkbMultiPolygon:
    case wkbTIN:
      return sdb::SmtFtTin;
    default:
      return sdb::SmtFtUnknown;
  }
}

void copy_smt_style_to_ogr(const base::SmtStyle* src, OGRFeature* dst) {
  if (!src || !dst) {
    return;
  }
  const int oi = dst->GetFieldIndex("style");
  if (oi < 0) {
    return;
  }
  dst->SetField(oi, static_cast<int>(sizeof(base::SmtStyle)),
                reinterpret_cast<const void*>(src));
}

base::SmtStyle* copy_ogr_style_from_ogr(OGRFeature* src) {
  if (!src) {
    return nullptr;
  }
  const int oi = src->GetFieldIndex("style");
  if (oi < 0) {
    return nullptr;
  }
  int nbytes = 0;
  GByte* data = src->GetFieldAsBinary(oi, &nbytes);
  if (!data || nbytes < static_cast<int>(sizeof(base::SmtStyle))) {
    return nullptr;
  }
  auto* style = new base::SmtStyle();
  std::memcpy(style, data, sizeof(base::SmtStyle));
  return style;
}

bool copy_ogr_feature_to_smt(OGRFeature* src, sdb::SmtFeature* dst) {
  if (!src || !dst) {
    return false;
  }
  const sdb::SmtFeatureType ft = infer_feature_type(src, sdb::SmtFtUnknown);
  dst->SetFeatureType(ft);
  dst->SetID(static_cast<long>(src->GetFID()));

  if (ft == sdb::SmtFtGrid) {
    geo::Grid* grid = decode_smt_grid(src);
    if (!grid) {
      return false;
    }
    dst->SetGeometry(grid);
  } else if (ft == sdb::SmtFtTin) {
    geo::Tin* tin = decode_smt_tin(src);
    if (!tin) {
      return false;
    }
    dst->SetGeometry(tin);
  } else {
    OGRGeometry* geom = decode_ogr_geometry(src, ft);
    if (!geom) {
      return false;
    }
    dst->SetGeometryDirectly(geom);
  }

  if (base::SmtStyle* sty = copy_ogr_style_from_ogr(src)) {
    dst->SetStyle(sty);
  }
  return true;
}

bool feature_kind_traits<sdb::SmtFtDot>::encode_geom(const OGRGeometry* src,
                                                        OGRFeature* dst) {
  return encode_point(src, dst);
}

OGRGeometry* feature_kind_traits<sdb::SmtFtDot>::decode_geom(
    OGRFeature* src) {
  return decode_point(src);
}

bool feature_kind_traits<sdb::SmtFtCurve>::encode_geom(
    const OGRGeometry* src, OGRFeature* dst) {
  return encode_linestring(src, dst);
}

OGRGeometry* feature_kind_traits<sdb::SmtFtCurve>::decode_geom(
    OGRFeature* src) {
  return decode_linestring(src);
}

bool feature_kind_traits<sdb::SmtFtSurface>::encode_geom(
    const OGRGeometry* src, OGRFeature* dst) {
  return encode_polygon(src, dst);
}

OGRGeometry* feature_kind_traits<sdb::SmtFtSurface>::decode_geom(
    OGRFeature* src) {
  return decode_polygon(src);
}

bool feature_kind_traits<sdb::SmtFtAnno>::encode_geom(const OGRGeometry* src,
                                                         OGRFeature* dst) {
  return encode_point(src, dst);
}

OGRGeometry* feature_kind_traits<sdb::SmtFtAnno>::decode_geom(
    OGRFeature* src) {
  return decode_point(src);
}

bool encode_tin_mesh(const geo::Tin* tin, OGRFeature* dst) {
  if (!tin) {
    return false;
  }
  return dst->SetGeometry(&tin->ogr()) == OGRERR_NONE;
}

bool feature_kind_traits<sdb::SmtFtTin>::encode_geom(const OGRGeometry* src,
                                                        OGRFeature* dst) {
  return src && dst && dst->SetGeometry(src) == OGRERR_NONE;
}

OGRGeometry* feature_kind_traits<sdb::SmtFtTin>::decode_geom(
    OGRFeature* src) {
  OGRGeometry* geom = src->GetGeometryRef();
  return geom ? geom->clone() : nullptr;
}

bool encode_grid_mesh(const geo::Grid* grid, OGRFeature* dst) {
  if (!grid || grid->is_empty()) {
    return false;
  }
  return dst->SetGeometry(&grid->ogr()) == OGRERR_NONE;
}

bool feature_kind_traits<sdb::SmtFtGrid>::encode_geom(const OGRGeometry* src,
                                                         OGRFeature* dst) {
  return src && dst && dst->SetGeometry(src) == OGRERR_NONE;
}

OGRGeometry* feature_kind_traits<sdb::SmtFtGrid>::decode_geom(
    OGRFeature* src) {
  OGRGeometry* geom = src->GetGeometryRef();
  return geom ? geom->clone() : nullptr;
}

bool feature_kind_traits<sdb::SmtFtChildImage>::encode_geom(
    const OGRGeometry* /*src*/, OGRFeature* /*dst*/) {
  return false;
}

OGRGeometry* feature_kind_traits<sdb::SmtFtChildImage>::decode_geom(
    OGRFeature* /*src*/) {
  return nullptr;
}

bool encode_smt_geometry(const OGRGeometry* src, OGRFeature* dst,
                         sdb::SmtFeatureType ft) {
  if (!src || !dst) {
    return false;
  }
  return visit_feature_kind(ft, [&](auto traits) {
    using Traits = decltype(traits);
    return Traits::encode_geom(src, dst);
  });
}

bool encode_smt_geometry(const geo::Tin* src, OGRFeature* dst,
                         sdb::SmtFeatureType /*ft*/) {
  return encode_tin_mesh(src, dst);
}

bool encode_smt_geometry(const geo::Grid* src, OGRFeature* dst,
                         sdb::SmtFeatureType /*ft*/) {
  return encode_grid_mesh(src, dst);
}

OGRGeometry* decode_ogr_geometry(OGRFeature* src,
                                 sdb::SmtFeatureType hint) {
  if (!src) {
    return nullptr;
  }
  const sdb::SmtFeatureType ft = infer_feature_type(src, hint);
  OGRGeometry* out = nullptr;
  visit_feature_kind(ft, [&](auto traits) {
    using Traits = decltype(traits);
    out = Traits::decode_geom(src);
    return out != nullptr;
  });
  return out;
}

geo::Tin* decode_smt_tin(OGRFeature* src) {
  if (!src) {
    return nullptr;
  }
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom) {
    return nullptr;
  }
  auto add_triangle_from_ring = [](geo::Tin* tin, OGRLinearRing* ring) {
    if (!ring || ring->getNumPoints() < 3) {
      return;
    }
    OGRPoint p0(ring->getX(0), ring->getY(0));
    OGRPoint p1(ring->getX(1), ring->getY(1));
    OGRPoint p2(ring->getX(2), ring->getY(2));
    tin->add_point(&p0);
    const int ia = tin->get_point_count() - 1;
    tin->add_point(&p1);
    const int ib = tin->get_point_count() - 1;
    tin->add_point(&p2);
    const int ic = tin->get_point_count() - 1;
    base::SmtTriangle tri;
    tri.a = ia;
    tri.b = ib;
    tri.c = ic;
    tin->add_triangle(&tri);
  };

  auto* tin = new geo::Tin();
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (wt == wkbMultiPolygon) {
    auto* multi = geom->toMultiPolygon();
    const int n = multi->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      OGRPolygon* poly = multi->getGeometryRef(i)->toPolygon();
      add_triangle_from_ring(tin, poly ? poly->getExteriorRing() : nullptr);
    }
  } else if (wt == wkbPolygon || wt == wkbTriangle) {
    add_triangle_from_ring(tin, geom->toPolygon()->getExteriorRing());
  } else if (wt == wkbTIN) {
    auto* coll = geom->toGeometryCollection();
    const int n = coll ? coll->getNumGeometries() : 0;
    for (int i = 0; i < n && coll; ++i) {
      OGRGeometry* part = coll->getGeometryRef(i);
      if (part && (wkbFlatten(part->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(part->getGeometryType()) == wkbTriangle)) {
        add_triangle_from_ring(tin, part->toPolygon()->getExteriorRing());
      }
    }
  } else {
    delete tin;
    return nullptr;
  }
  if (tin->get_triangle_count() < 1 && tin->get_point_count() < 3) {
    delete tin;
    return nullptr;
  }
  return tin;
}

geo::Grid* decode_smt_grid(OGRFeature* src) {
  if (!src) {
    return nullptr;
  }
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
  auto* grid = new geo::Grid(rows, cols);
  int k = 0;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols && k < n; ++c, ++k) {
      OGRPoint* pt = mp->getGeometryRef(k)->toPoint();
      geo::RawPoint raw;
      raw.x = pt->getX();
      raw.y = pt->getY();
      grid->set_node(r, c, raw);
    }
  }
  return grid;
}

bool create_vector_layer(GDALDataset* ds, const char* name,
                         sdb::SmtFeatureType ft, OGRLayer** out) {
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
