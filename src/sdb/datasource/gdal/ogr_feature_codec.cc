// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "sdb/datasource/gdal/ogr_feature_kind.h"

#include "feature.h"
#include "geometry.h"

#include "ogrsf_frmts.h"

#include <cstring>

namespace sdb {
namespace datasource {

namespace {

void force_feature_type(Smt_GIS::SmtFeature* dst, Smt_GIS::SmtFeatureType type) {
  dst->SetFeatureType(Smt_GIS::SmtFtUnknown);
  dst->SetFeatureType(type);
}

Smt_GIS::SmtFeatureType infer_feature_type(OGRFeature* src) {
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
    const Smt_GIS::SmtFeature* /*src*/, OGRFeature* /*dst*/) {
  return false;
}

bool feature_kind_traits<Smt_GIS::SmtFtTin>::decode_geom(OGRFeature* /*src*/,
                                                        Smt_GIS::SmtFeature* /*dst*/) {
  return false;
}

bool feature_kind_traits<Smt_GIS::SmtFtGrid>::encode_geom(
    const Smt_GIS::SmtFeature* /*src*/, OGRFeature* /*dst*/) {
  return false;
}

bool feature_kind_traits<Smt_GIS::SmtFtGrid>::decode_geom(
    OGRFeature* /*src*/, Smt_GIS::SmtFeature* /*dst*/) {
  return false;
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
  if (!src || !dst) {
    return false;
  }
  const Smt_GIS::SmtFeatureType ft = infer_feature_type(src);
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
