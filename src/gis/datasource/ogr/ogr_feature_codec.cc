// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/ogr/ogr_feature_codec.h"

#include "gis/geo/ops/indexed_tin.h"
#include "base/memory/arena.h"
#include "gis/datasource/ogr/ogr_feature_kind.h"

#include <cstring>

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace datasource {

namespace {

bool field_nonempty(OGRFeature* src, const char* name) {
  if (!src || !name) {
    return false;
  }
  const int i = src->GetFieldIndex(name);
  if (i < 0) {
    return false;
  }
  const char* v = src->GetFieldAsString(i);
  return v && v[0];
}

OGRPoint* first_point(OGRGeometry* geom) {
  if (!geom) {
    return nullptr;
  }
  if (auto* pt = dynamic_cast<OGRPoint*>(geom)) {
    return pt;
  }
  if (auto* multi = dynamic_cast<OGRMultiPoint*>(geom)) {
    if (multi->getNumGeometries() > 0) {
      return dynamic_cast<OGRPoint*>(multi->getGeometryRef(0));
    }
  }
  return nullptr;
}

OGRLineString* first_linestring(OGRGeometry* geom) {
  if (!geom) {
    return nullptr;
  }
  if (auto* line = dynamic_cast<OGRLineString*>(geom)) {
    return line;
  }
  if (auto* multi = dynamic_cast<OGRMultiLineString*>(geom)) {
    if (multi->getNumGeometries() > 0) {
      return dynamic_cast<OGRLineString*>(multi->getGeometryRef(0));
    }
  }
  return nullptr;
}

OGRPolygon* first_polygon(OGRGeometry* geom) {
  if (!geom) {
    return nullptr;
  }
  if (auto* poly = dynamic_cast<OGRPolygon*>(geom)) {
    return poly;
  }
  if (auto* multi = dynamic_cast<OGRMultiPolygon*>(geom)) {
    if (multi->getNumGeometries() > 0) {
      return dynamic_cast<OGRPolygon*>(multi->getGeometryRef(0));
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
  OGRGeometry* geom = src ? src->GetGeometryRef() : nullptr;
  if (!geom) {
    return nullptr;
  }
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (wt == wkbLineString || wt == wkbMultiLineString) {
    return geom->clone();
  }
  OGRLineString* po_line = first_linestring(geom);
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
  OGRGeometry* geom = src ? src->GetGeometryRef() : nullptr;
  if (!geom) {
    return nullptr;
  }
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (wt == wkbPolygon || wt == wkbMultiPolygon) {
    return geom->clone();
  }
  OGRPolygon* po_poly = first_polygon(geom);
  return po_poly ? po_poly->clone() : nullptr;
}

}  // namespace

gis::VectorSchema infer_vector_schema(OGRFeature* src,
                                      gis::VectorSchema hint) {
  if (hint != gis::VectorSchema::kNone) {
    return hint;
  }
  if (!src) {
    return gis::VectorSchema::kNone;
  }
  if (field_nonempty(src, field_anno::name)) {
    return gis::VectorSchema::kAnno;
  }
  if (src->GetFieldIndex(field_tin::name) >= 0) {
    return gis::VectorSchema::kTin;
  }
  if (src->GetFieldIndex(field_grid_row::name) >= 0) {
    return gis::VectorSchema::kGrid;
  }
  OGRGeometry* geom = src->GetGeometryRef();
  if (geom) {
    const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
    if (wt == wkbTIN || wt == wkbTriangle) {
      return gis::VectorSchema::kTin;
    }
  }
  return gis::VectorSchema::kNone;
}

OGRwkbGeometryType infer_geometry_type(OGRFeature* src) {
  if (!src || !src->GetGeometryRef()) {
    return wkbUnknown;
  }
  return src->GetGeometryRef()->getGeometryType();
}

bool copy_ogr_feature_to_feature(OGRFeature* src, gis::Feature* dst) {
  if (!src || !dst) {
    return false;
  }
  OGRFeature* clone = src->Clone();
  if (!clone) {
    return false;
  }
  dst->reset_ogr(clone, true);
  return true;
}

bool encode_tin_mesh(const OGRTriangulatedSurface* tin, OGRFeature* dst) {
  if (!tin || !dst) {
    return false;
  }
  OGRMultiPolygon mp;
  auto* writable = const_cast<OGRTriangulatedSurface*>(tin);
  const int n = writable->getNumGeometries();
  for (int i = 0; i < n; ++i) {
    OGRPolygon* patch = writable->getGeometryRef(i);
    if (patch == nullptr) {
      continue;
    }
    OGRLinearRing* src = patch->getExteriorRing();
    if (src == nullptr || src->getNumPoints() < 3) {
      continue;
    }
    OGRPoint a;
    OGRPoint b;
    OGRPoint c;
    src->getPoint(0, &a);
    src->getPoint(1, &b);
    src->getPoint(2, &c);
    // Four unique vertices so GDAL does not promote the part to Triangle/TIN
    // (GPKG's non-standard TIN writer aborts in debug down_cast).
    OGRLinearRing ring;
    ring.addPoint(a.getX(), a.getY(), a.getZ());
    ring.addPoint(b.getX(), b.getY(), b.getZ());
    ring.addPoint(c.getX(), c.getY(), c.getZ());
    ring.addPoint((a.getX() + b.getX() + c.getX()) / 3.0,
                  (a.getY() + b.getY() + c.getY()) / 3.0,
                  (a.getZ() + b.getZ() + c.getZ()) / 3.0);
    ring.closeRings();
    OGRPolygon poly;
    poly.addRing(&ring);
    if (mp.addGeometry(&poly) != OGRERR_NONE) {
      return false;
    }
  }
  const int ti = dst->GetFieldIndex("tin");
  if (ti >= 0) {
    dst->SetField(ti, 1);
  }
  return dst->SetGeometry(&mp) == OGRERR_NONE;
}

bool encode_tin_geom(const OGRGeometry* src, OGRFeature* dst) {
  if (!src || !dst) {
    return false;
  }
  const int ti = dst->GetFieldIndex("tin");
  if (ti >= 0) {
    dst->SetField(ti, 1);
  }
  const OGRwkbGeometryType wt = wkbFlatten(src->getGeometryType());
  if (wt == wkbTIN || wt == wkbTriangle) {
    if (auto* tin = dynamic_cast<const OGRTriangulatedSurface*>(src)) {
      return encode_tin_mesh(tin, dst);
    }
  }
  if (wt == wkbMultiPolygon || wt == wkbPolygon) {
    return dst->SetGeometry(src) == OGRERR_NONE;
  }
  OGRMultiPolygon mp;
  if (auto* coll = dynamic_cast<const OGRGeometryCollection*>(src)) {
    const int n = coll->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      auto* poly = dynamic_cast<OGRPolygon*>(
          const_cast<OGRGeometryCollection*>(coll)->getGeometryRef(i));
      if (poly && mp.addGeometry(poly) != OGRERR_NONE) {
        return false;
      }
    }
    return dst->SetGeometry(&mp) == OGRERR_NONE;
  }
  return dst->SetGeometry(src) == OGRERR_NONE;
}

OGRGeometry* decode_by_wkb(OGRFeature* src, OGRwkbGeometryType wkb) {
  switch (wkbFlatten(wkb)) {
    case wkbPoint:
    case wkbMultiPoint:
      return decode_point(src);
    case wkbLineString:
    case wkbMultiLineString:
      return decode_linestring(src);
    case wkbPolygon:
    case wkbLinearRing:
    case wkbMultiPolygon:
    case wkbTIN:
    case wkbTriangle:
      return decode_polygon(src);
    default: {
      OGRGeometry* geom = src ? src->GetGeometryRef() : nullptr;
      return geom ? geom->clone() : nullptr;
    }
  }
}

bool encode_ogr_geometry(const OGRGeometry* src, OGRFeature* dst,
                         OGRwkbGeometryType wkb, gis::VectorSchema schema) {
  if (!src || !dst) {
    return false;
  }
  if (schema == gis::VectorSchema::kChildImage) {
    return false;
  }
  if (schema == gis::VectorSchema::kTin) {
    return encode_tin_geom(src, dst);
  }
  if (schema == gis::VectorSchema::kGrid) {
    return dst->SetGeometry(src) == OGRERR_NONE;
  }
  if (schema == gis::VectorSchema::kAnno) {
    return encode_point(src, dst);
  }
  const OGRwkbGeometryType use =
      (wkb != wkbUnknown && wkb != wkbNone) ? wkb : src->getGeometryType();
  switch (wkbFlatten(use)) {
    case wkbPoint:
      return encode_point(src, dst);
    case wkbLineString:
    case wkbMultiLineString:
      return encode_linestring(src, dst);
    case wkbPolygon:
    case wkbLinearRing:
    case wkbMultiPolygon:
      return encode_polygon(src, dst);
    case wkbTIN:
    case wkbTriangle:
      return encode_tin_geom(src, dst);
    default:
      return dst->SetGeometry(src) == OGRERR_NONE;
  }
}

OGRGeometry* decode_ogr_geometry(OGRFeature* src, gis::VectorSchema hint) {
  if (!src) {
    return nullptr;
  }
  thread_local size_t tls_decode_scratch_ticks = 0;
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    if ((++tls_decode_scratch_ticks % 64) == 0) {
      tls->clear(64 * 1024);
    }
  }
  const gis::VectorSchema schema = infer_vector_schema(src, hint);
  if (schema == gis::VectorSchema::kChildImage) {
    return nullptr;
  }
  if (schema == gis::VectorSchema::kAnno) {
    return decode_point(src);
  }
  if (schema == gis::VectorSchema::kTin || schema == gis::VectorSchema::kGrid) {
    OGRGeometry* geom = src->GetGeometryRef();
    return geom ? geom->clone() : nullptr;
  }
  return decode_by_wkb(src, infer_geometry_type(src));
}

bool create_vector_layer(GDALDataset* ds, const char* name,
                         OGRwkbGeometryType wkb, gis::VectorSchema schema,
                         OGRLayer** out) {
  if (!ds || !name || !out) {
    return false;
  }
  *out = nullptr;
  return visit_vector_schema(schema, [&](auto traits) {
    using Traits = decltype(traits);
    if (Traits::is_raster) {
      return false;
    }
    const OGRwkbGeometryType use_wkb =
        schema == gis::VectorSchema::kNone ? wkb : Traits::wkb;
    OGRLayer* lyr = ds->CreateLayer(name, nullptr, use_wkb, nullptr);
    if (!lyr) {
      return false;
    }
    OGRFieldDefn style("style", OFTBinary);
    lyr->CreateField(&style);
    for_each_extra_field<typename Traits::extra_fields>([&](auto field) {
      OGRFieldDefn defn(field.name, field.ogr_type);
      lyr->CreateField(&defn);
    });
    if (schema == gis::VectorSchema::kNone) {
      switch (wkbFlatten(wkb)) {
        case wkbLineString:
        case wkbMultiLineString: {
          OGRFieldDefn length(field_length::name, field_length::ogr_type);
          lyr->CreateField(&length);
          break;
        }
        case wkbPolygon:
        case wkbMultiPolygon: {
          OGRFieldDefn area(field_area::name, field_area::ogr_type);
          lyr->CreateField(&area);
          break;
        }
        default:
          break;
      }
    }
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
}  // namespace gis
