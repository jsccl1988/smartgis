// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_FEATURE_KIND_H_
#define SDB_DATASOURCE_GDAL_OGR_FEATURE_KIND_H_

#include <tuple>
#include <utility>

#include "gis/map/layer_kind.h"
#include "ogr_core.h"
#include "ogrsf_frmts.h"

class OGRFeature;
class OGRGeometry;

namespace gis {
namespace datasource {

struct field_anno {
  static constexpr char name[] = "anno";
  static constexpr OGRFieldType ogr_type = OFTString;
};

struct field_color {
  static constexpr char name[] = "color";
  static constexpr OGRFieldType ogr_type = OFTInteger;
};

struct field_angle {
  static constexpr char name[] = "angle";
  static constexpr OGRFieldType ogr_type = OFTReal;
};

struct field_length {
  static constexpr char name[] = "length";
  static constexpr OGRFieldType ogr_type = OFTReal;
};

struct field_area {
  static constexpr char name[] = "area";
  static constexpr OGRFieldType ogr_type = OFTReal;
};

struct field_grid_row {
  static constexpr char name[] = "grid_row";
  static constexpr OGRFieldType ogr_type = OFTInteger;
};

struct field_grid_col {
  static constexpr char name[] = "grid_col";
  static constexpr OGRFieldType ogr_type = OFTInteger;
};

// Marks a MultiPolygon layer as a triangle mesh. GPKG has no stable TIN type
// (gpkg_geom_TIN is non-standard and aborts in debug GDAL down_cast).
struct field_tin {
  static constexpr char name[] = "tin";
  static constexpr OGRFieldType ogr_type = OFTInteger;
};

template <gis::VectorSchema Schema>
struct vector_schema_traits;

template <>
struct vector_schema_traits<gis::VectorSchema::kNone> {
  static constexpr OGRwkbGeometryType wkb = wkbUnknown;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<>;
};

template <>
struct vector_schema_traits<gis::VectorSchema::kAnno> {
  static constexpr OGRwkbGeometryType wkb = wkbPoint;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_anno, field_color, field_angle>;
};

template <>
struct vector_schema_traits<gis::VectorSchema::kTin> {
  static constexpr OGRwkbGeometryType wkb = wkbMultiPolygon;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_tin>;
};

template <>
struct vector_schema_traits<gis::VectorSchema::kGrid> {
  static constexpr OGRwkbGeometryType wkb = wkbMultiPoint;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_grid_row, field_grid_col>;
};

template <>
struct vector_schema_traits<gis::VectorSchema::kChildImage> {
  static constexpr OGRwkbGeometryType wkb = wkbNone;
  static constexpr bool is_raster = true;
  using extra_fields = std::tuple<>;
};

template <typename Fn>
bool visit_vector_schema(gis::VectorSchema schema, Fn&& fn) {
  switch (schema) {
    case gis::VectorSchema::kNone:
      return fn(vector_schema_traits<gis::VectorSchema::kNone>{});
    case gis::VectorSchema::kAnno:
      return fn(vector_schema_traits<gis::VectorSchema::kAnno>{});
    case gis::VectorSchema::kTin:
      return fn(vector_schema_traits<gis::VectorSchema::kTin>{});
    case gis::VectorSchema::kGrid:
      return fn(vector_schema_traits<gis::VectorSchema::kGrid>{});
    case gis::VectorSchema::kChildImage:
      return fn(vector_schema_traits<gis::VectorSchema::kChildImage>{});
    default:
      return false;
  }
}

template <typename Tuple, typename Fn, std::size_t... I>
void for_each_extra_field(Fn&& fn, std::index_sequence<I...>) {
  (fn(std::tuple_element_t<I, Tuple>{}), ...);
}

template <typename Tuple, typename Fn>
void for_each_extra_field(Fn&& fn) {
  for_each_extra_field<Tuple>(
      std::forward<Fn>(fn),
      std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

inline OGRwkbGeometryType layer_geometry_type(OGRLayer* layer) {
  if (!layer) {
    return wkbUnknown;
  }
  return layer->GetGeomType();
}

inline gis::VectorSchema layer_vector_schema(OGRLayer* layer) {
  if (!layer) {
    return gis::VectorSchema::kNone;
  }
  if (layer->FindFieldIndex(field_anno::name, TRUE) >= 0) {
    return gis::VectorSchema::kAnno;
  }
  if (layer->FindFieldIndex(field_grid_row::name, TRUE) >= 0) {
    return gis::VectorSchema::kGrid;
  }
  if (layer->FindFieldIndex(field_tin::name, TRUE) >= 0) {
    return gis::VectorSchema::kTin;
  }
  const OGRwkbGeometryType wkb = wkbFlatten(layer->GetGeomType());
  if (wkb == wkbTIN || wkb == wkbTriangle) {
    return gis::VectorSchema::kTin;
  }
  return gis::VectorSchema::kNone;
}

inline OGRwkbGeometryType wkb_for_vector_schema(gis::VectorSchema schema,
                                                OGRwkbGeometryType fallback) {
  switch (schema) {
    case gis::VectorSchema::kAnno:
      return wkbPoint;
    case gis::VectorSchema::kGrid:
      return wkbMultiPoint;
    case gis::VectorSchema::kTin:
      return wkbMultiPolygon;
    case gis::VectorSchema::kChildImage:
      return wkbNone;
    default:
      return fallback;
  }
}

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_OGR_FEATURE_KIND_H_
