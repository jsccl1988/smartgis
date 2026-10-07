// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Map-space OGR feature decode used by content ingest. Ring decimation and
// the Y-flip live here so hosted MapLayer / MapFeature stay a thin adapter.
// Header-only, same link story as feature_load_pipeline.h (do not put these
// symbols in gis.dll and a second PE).

#ifndef GIS_DATASOURCE_PIPELINE_OGR_FEATURE_LOAD_H_
#define GIS_DATASOURCE_PIPELINE_OGR_FEATURE_LOAD_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "gis/datasource/ogr/ogr_text_encoding.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace datasource {

// Cap ring density so GDI Polygon / paint stays bounded on prefecture packs.
// 4096 keeps coasts/islands denser than the old 2048 cap (blocky Taiwan).
inline constexpr int k_max_ring_points = 4096;

// Attribute cap copied from each OGR feature (hosted tables stay small).
inline constexpr int k_max_ogr_fields = 12;

// Map-space vertex after OGR decode. Y is flipped so +Y is south.
struct OgrMapVertex {
  double x = 0;
  double y = 0;
};

// UTF-8 name/value copied from one OGR field.
struct OgrMapField {
  std::string name;
  std::string value;
};

// Geometry class before hosted kind / anno overrides. No text kind here.
enum class OgrPartKind { kPoint, kLine, kPolygon };

// One drawable part. MultiLineString and MultiPolygon expand to N parts.
// Polygon parts carry the exterior ring only (holes are not separate parts).
struct OgrFeaturePart {
  OgrPartKind kind = OgrPartKind::kPoint;
  std::vector<OgrMapVertex> points;
  std::vector<OgrMapField> fields;
};

namespace detail {

inline void copy_ogr_fields(OGRFeature* feat, std::vector<OgrMapField>* out) {
  if (!feat || !out) {
    return;
  }
  out->clear();
  OGRFeatureDefn* defn = feat->GetDefnRef();
  if (!defn) {
    return;
  }
  const int field_count = defn->GetFieldCount();
  for (int i = 0; i < field_count &&
                  static_cast<int>(out->size()) < k_max_ogr_fields;
       ++i) {
    if (!feat->IsFieldSetAndNotNull(i)) {
      continue;
    }
    OGRFieldDefn* fd = defn->GetFieldDefn(i);
    if (!fd) {
      continue;
    }
    const char* fname = fd->GetNameRef();
    out->push_back(
        {fname && fname[0] ? fname : "field",
         ogr_bytes_to_utf8(feat->GetFieldAsString(i))});
  }
}

inline OgrFeaturePart make_part(OGRFeature* feat, OgrPartKind kind,
                                std::vector<OgrMapVertex> points) {
  OgrFeaturePart part;
  part.kind = kind;
  part.points = std::move(points);
  copy_ogr_fields(feat, &part.fields);
  return part;
}

}  // namespace detail

// Step-sample |ring| into map space (Y flipped). Appends; does not clear
// |out|. |max_points| below 1 is treated as 1. The last vertex is kept when
// the step would skip it.
inline void decimate_ogr_ring(OGRLineString* ring,
                              std::vector<OgrMapVertex>* out,
                              int max_points = k_max_ring_points) {
  if (!ring || !out) {
    return;
  }
  const int n = ring->getNumPoints();
  if (n <= 0) {
    return;
  }
  if (max_points < 1) {
    max_points = 1;
  }
  int step = 1;
  if (n > max_points) {
    step = n / max_points;
    if (step < 1) {
      step = 1;
    }
  }
  for (int i = 0; i < n; i += step) {
    out->push_back({ring->getX(i), -ring->getY(i)});
  }
  if ((n - 1) % step != 0) {
    out->push_back({ring->getX(n - 1), -ring->getY(n - 1)});
  }
}

// Expand one OGR feature into drawable parts. Clears |out|. Returns 0 when
// the geometry is missing, empty, or not a point/line/polygon (or multi of
// those). Does not apply hosted kind overrides or china line clips.
inline size_t load_ogr_feature_parts(OGRFeature* feat,
                                     std::vector<OgrFeaturePart>* out) {
  if (!out) {
    return 0;
  }
  out->clear();
  if (!feat) {
    return 0;
  }
  OGRGeometry* geom = feat->GetGeometryRef();
  if (!geom || geom->IsEmpty()) {
    return 0;
  }

  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  if (flat == wkbPoint) {
    auto* pt = geom->toPoint();
    std::vector<OgrMapVertex> points;
    points.push_back({pt->getX(), -pt->getY()});
    out->push_back(
        detail::make_part(feat, OgrPartKind::kPoint, std::move(points)));
    return 1;
  }
  if (flat == wkbLineString || flat == wkbLinearRing) {
    std::vector<OgrMapVertex> points;
    decimate_ogr_ring(geom->toLineString(), &points);
    if (points.size() < 2) {
      return 0;
    }
    out->push_back(
        detail::make_part(feat, OgrPartKind::kLine, std::move(points)));
    return 1;
  }
  if (flat == wkbPolygon) {
    std::vector<OgrMapVertex> points;
    if (OGRLinearRing* ext = geom->toPolygon()->getExteriorRing()) {
      decimate_ogr_ring(ext, &points);
    }
    if (points.size() < 3) {
      return 0;
    }
    out->push_back(
        detail::make_part(feat, OgrPartKind::kPolygon, std::move(points)));
    return 1;
  }
  if (flat == wkbMultiPoint) {
    auto* multi = geom->toMultiPoint();
    if (!multi || multi->getNumGeometries() < 1) {
      return 0;
    }
    auto* pt = multi->getGeometryRef(0)->toPoint();
    std::vector<OgrMapVertex> points;
    points.push_back({pt->getX(), -pt->getY()});
    out->push_back(
        detail::make_part(feat, OgrPartKind::kPoint, std::move(points)));
    return 1;
  }
  if (flat == wkbMultiLineString) {
    auto* multi = geom->toMultiLineString();
    if (!multi || multi->getNumGeometries() < 1) {
      return 0;
    }
    const int ngeom = multi->getNumGeometries();
    out->reserve(static_cast<size_t>(ngeom));
    for (int i = 0; i < ngeom; ++i) {
      OGRGeometry* part = multi->getGeometryRef(i);
      if (!part || wkbFlatten(part->getGeometryType()) != wkbLineString) {
        continue;
      }
      std::vector<OgrMapVertex> points;
      decimate_ogr_ring(part->toLineString(), &points);
      if (points.size() < 2) {
        continue;
      }
      out->push_back(
          detail::make_part(feat, OgrPartKind::kLine, std::move(points)));
    }
    return out->size();
  }
  if (flat == wkbMultiPolygon) {
    auto* multi = geom->toMultiPolygon();
    if (!multi || multi->getNumGeometries() < 1) {
      return 0;
    }
    const int ngeom = multi->getNumGeometries();
    out->reserve(static_cast<size_t>(ngeom));
    for (int i = 0; i < ngeom; ++i) {
      OGRGeometry* part = multi->getGeometryRef(i);
      if (!part || wkbFlatten(part->getGeometryType()) != wkbPolygon) {
        continue;
      }
      std::vector<OgrMapVertex> points;
      if (OGRLinearRing* ext = part->toPolygon()->getExteriorRing()) {
        decimate_ogr_ring(ext, &points);
      }
      if (points.size() < 3) {
        continue;
      }
      out->push_back(
          detail::make_part(feat, OgrPartKind::kPolygon, std::move(points)));
    }
    return out->size();
  }
  return 0;
}

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_PIPELINE_OGR_FEATURE_LOAD_H_
