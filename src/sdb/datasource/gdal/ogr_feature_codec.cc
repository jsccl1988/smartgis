// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "sdb/datasource/gdal/ogr_feature_kind.h"

#include "feature.h"
#include "geometry.h"
#include "matrix2d.h"

#include "ogrsf_frmts.h"

#include <cstring>

namespace sdb {
namespace datasource {

namespace {

void force_feature_type(Smt_GIS::SmtFeature* dst, Smt_GIS::SmtFeatureType type) {
  dst->SetFeatureType(Smt_GIS::SmtFtUnknown);
  dst->SetFeatureType(type);
}

Smt_GIS::SmtFeatureType infer_feature_type(OGRFeature* src,
                                           Smt_GIS::SmtFeatureType hint) {
  if (hint == Smt_GIS::SmtFtTin || hint == Smt_GIS::SmtFtGrid ||
      hint == Smt_GIS::SmtFtAnno || hint == Smt_GIS::SmtFtDot ||
      hint == Smt_GIS::SmtFtCurve || hint == Smt_GIS::SmtFtSurface) {
    return hint;
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

bool encode_point(const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  const auto* pt =
      dynamic_cast<const Smt_Geo::SmtPoint*>(src->GetGeometryRef());
  if (!pt) {
    return false;
  }
  OGRPoint ogr_pt(pt->GetX(), pt->GetY());
  return dst->SetGeometry(&ogr_pt) == OGRERR_NONE;
}

bool decode_point(OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom || wkbFlatten(geom->getGeometryType()) != wkbPoint) {
    return false;
  }
  auto* po_point = geom->toPoint();
  dst->SetGeometryDirectly(new Smt_Geo::SmtPoint(po_point->getX(), po_point->getY()));
  return true;
}

bool encode_linestring(const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  const auto* line =
      dynamic_cast<const Smt_Geo::SmtLineString*>(src->GetGeometryRef());
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

bool decode_linestring(OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom || wkbFlatten(geom->getGeometryType()) != wkbLineString) {
    return false;
  }
  auto* po_line = geom->toLineString();
  auto* line = new Smt_Geo::SmtLineString();
  const int n = po_line->getNumPoints();
  line->SetNumPoints(n);
  for (int i = 0; i < n; ++i) {
    line->SetPoint(i, po_line->getX(i), po_line->getY(i));
  }
  dst->SetGeometryDirectly(line);
  return true;
}

bool encode_polygon(const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  const auto* poly =
      dynamic_cast<const Smt_Geo::SmtPolygon*>(src->GetGeometryRef());
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

bool decode_polygon(OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom || wkbFlatten(geom->getGeometryType()) != wkbPolygon) {
    return false;
  }
  auto* po_poly = geom->toPolygon();
  OGRLinearRing* po_ring = po_poly->getExteriorRing();
  if (!po_ring) {
    return false;
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
  dst->SetGeometryDirectly(poly);
  return true;
}

template <typename Field>
void write_extra_field(const Smt_GIS::SmtFeature* src, OGRFeature* dst, Field) {
  const int si =
      const_cast<Smt_GIS::SmtFeature*>(src)->GetFieldIndexByName(Field::name);
  const int oi = dst->GetFieldIndex(Field::name);
  if (si < 0 || oi < 0) {
    return;
  }
  const Smt_GIS::SmtAttribute* att = src->GetAttributeRef();
  const Smt_GIS::SmtField* f = att ? att->GetFieldPtr(si) : nullptr;
  if (!f) {
    return;
  }
  if constexpr (Field::ogr_type == OFTString) {
    dst->SetField(oi, f->GetValueAsString());
  } else if constexpr (Field::ogr_type == OFTInteger) {
    dst->SetField(oi, f->GetValueAsInteger());
  } else if constexpr (Field::ogr_type == OFTReal) {
    dst->SetField(oi, f->GetValueAsDouble());
  }
}

template <typename Field>
void read_extra_field(OGRFeature* src, Smt_GIS::SmtFeature* dst, Field) {
  const int oi = src->GetFieldIndex(Field::name);
  if (oi < 0) {
    return;
  }
  if (dst->GetFieldIndexByName(Field::name) < 0) {
    Smt_GIS::SmtField fld;
    fld.SetName(Field::name);
    fld.SetType(static_cast<varType>(Field::smt_type));
    dst->AddField(fld);
  }
  const int si = dst->GetFieldIndexByName(Field::name);
  if (si < 0) {
    return;
  }
  if constexpr (Field::ogr_type == OFTString) {
    dst->SetFieldValue(si, src->GetFieldAsString(oi));
  } else if constexpr (Field::ogr_type == OFTInteger) {
    dst->SetFieldValue(si, src->GetFieldAsInteger(oi));
  } else if constexpr (Field::ogr_type == OFTReal) {
    dst->SetFieldValue(si, src->GetFieldAsDouble(oi));
  }
}

void copy_smt_attrs_by_name(const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  const Smt_GIS::SmtAttribute* att = src->GetAttributeRef();
  if (!att) {
    return;
  }
  const int n = att->GetFieldCount();
  for (int i = 0; i < n; ++i) {
    const Smt_GIS::SmtField* f = att->GetFieldPtr(i);
    if (!f) {
      continue;
    }
    const int oi = dst->GetFieldIndex(f->GetName());
    if (oi < 0) {
      continue;
    }
    switch (f->GetType()) {
      case Smt_Core::SmtInteger:
        dst->SetField(oi, f->GetValueAsInteger());
        break;
      case Smt_Core::SmtReal:
        dst->SetField(oi, f->GetValueAsDouble());
        break;
      case Smt_Core::SmtString:
        dst->SetField(oi, f->GetValueAsString());
        break;
      default:
        break;
    }
  }
}

void copy_ogr_attrs_by_name(OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  const int n = src->GetFieldCount();
  for (int i = 0; i < n; ++i) {
    OGRFieldDefn* defn = src->GetFieldDefnRef(i);
    if (!defn) {
      continue;
    }
    const char* name = defn->GetNameRef();
    if (dst->GetFieldIndexByName(name) < 0) {
      Smt_GIS::SmtField fld;
      fld.SetName(name);
      switch (defn->GetType()) {
        case OFTInteger:
          fld.SetType(Smt_Core::SmtInteger);
          break;
        case OFTReal:
          fld.SetType(Smt_Core::SmtReal);
          break;
        case OFTString:
        case OFTWideString:
          fld.SetType(Smt_Core::SmtString);
          break;
        default:
          fld.SetType(Smt_Core::SmtString);
          break;
      }
      dst->AddField(fld);
    }
    const int si = dst->GetFieldIndexByName(name);
    if (si < 0) {
      continue;
    }
    switch (defn->GetType()) {
      case OFTInteger:
        dst->SetFieldValue(si, src->GetFieldAsInteger(i));
        break;
      case OFTReal:
        dst->SetFieldValue(si, src->GetFieldAsDouble(i));
        break;
      case OFTString:
      case OFTWideString:
        dst->SetFieldValue(si, src->GetFieldAsString(i));
        break;
      default:
        break;
    }
  }
}

}  // namespace

bool feature_kind_traits<Smt_GIS::SmtFtDot>::encode_geom(
    const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  return encode_point(src, dst);
}

bool feature_kind_traits<Smt_GIS::SmtFtDot>::decode_geom(OGRFeature* src,
                                                        Smt_GIS::SmtFeature* dst) {
  return decode_point(src, dst);
}

bool feature_kind_traits<Smt_GIS::SmtFtCurve>::encode_geom(
    const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  return encode_linestring(src, dst);
}

bool feature_kind_traits<Smt_GIS::SmtFtCurve>::decode_geom(
    OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  return decode_linestring(src, dst);
}

bool feature_kind_traits<Smt_GIS::SmtFtSurface>::encode_geom(
    const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  return encode_polygon(src, dst);
}

bool feature_kind_traits<Smt_GIS::SmtFtSurface>::decode_geom(
    OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  return decode_polygon(src, dst);
}

bool feature_kind_traits<Smt_GIS::SmtFtAnno>::encode_geom(
    const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  return encode_point(src, dst);
}

bool feature_kind_traits<Smt_GIS::SmtFtAnno>::decode_geom(
    OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  return decode_point(src, dst);
}

bool feature_kind_traits<Smt_GIS::SmtFtTin>::encode_geom(
    const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  const auto* tin = dynamic_cast<const Smt_Geo::SmtTin*>(src->GetGeometryRef());
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

bool feature_kind_traits<Smt_GIS::SmtFtTin>::decode_geom(
    OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom) {
    return false;
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
  } else {
    delete tin;
    return false;
  }
  dst->SetGeometryDirectly(tin);
  return tin->GetTriangleCount() >= 1 || tin->GetPointCount() >= 3;
}

bool feature_kind_traits<Smt_GIS::SmtFtGrid>::encode_geom(
    const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  const auto* grid = dynamic_cast<const Smt_Geo::SmtGrid*>(src->GetGeometryRef());
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

bool feature_kind_traits<Smt_GIS::SmtFtGrid>::decode_geom(
    OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom || wkbFlatten(geom->getGeometryType()) != wkbMultiPoint) {
    return false;
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
  dst->SetGeometryDirectly(grid);
  return true;
}

bool feature_kind_traits<Smt_GIS::SmtFtChildImage>::encode_geom(
    const Smt_GIS::SmtFeature* /*src*/, OGRFeature* /*dst*/) {
  return false;
}

bool feature_kind_traits<Smt_GIS::SmtFtChildImage>::decode_geom(
    OGRFeature* /*src*/, Smt_GIS::SmtFeature* /*dst*/) {
  return false;
}

bool copy_smt_feature_to_ogr(const Smt_GIS::SmtFeature* src, OGRFeature* dst) {
  if (!src || !dst) {
    return false;
  }
  dst->SetFID(src->GetID());
  const bool ok = visit_feature_kind(src->GetFeatureType(), [&](auto traits) {
    using Traits = decltype(traits);
    if (!Traits::encode_geom(src, dst)) {
      return false;
    }
    for_each_extra_field<typename Traits::extra_fields>(
        [&](auto field) { write_extra_field(src, dst, field); });
    return true;
  });
  if (!ok) {
    return false;
  }
  copy_smt_attrs_by_name(src, dst);
  return true;
}

bool copy_ogr_feature_to_smt(OGRFeature* src, Smt_GIS::SmtFeature* dst) {
  return copy_ogr_feature_to_smt(src, dst, Smt_GIS::SmtFtUnknown);
}

bool copy_ogr_feature_to_smt(OGRFeature* src, Smt_GIS::SmtFeature* dst,
                             Smt_GIS::SmtFeatureType hint) {
  if (!src || !dst) {
    return false;
  }
  const Smt_GIS::SmtFeatureType ft = infer_feature_type(src, hint);
  if (ft == Smt_GIS::SmtFtUnknown) {
    return false;
  }
  force_feature_type(dst, ft);
  dst->SetID(static_cast<long>(src->GetFID()));
  const bool ok = visit_feature_kind(ft, [&](auto traits) {
    using Traits = decltype(traits);
    if (!Traits::decode_geom(src, dst)) {
      return false;
    }
    for_each_extra_field<typename Traits::extra_fields>(
        [&](auto field) { read_extra_field(src, dst, field); });
    return true;
  });
  if (!ok) {
    return false;
  }
  copy_ogr_attrs_by_name(src, dst);
  return true;
}

}  // namespace datasource
}  // namespace sdb
