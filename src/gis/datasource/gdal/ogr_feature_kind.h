// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_FEATURE_KIND_H_
#define SDB_DATASOURCE_GDAL_OGR_FEATURE_KIND_H_

#include <tuple>
#include <utility>

#include "base/core/bas_struct.h"
#include "ogr_core.h"
#include "gis/feature/feature.h"

class OGRFeature;

class OGRGeometry;

namespace gis {
namespace datasource {

struct field_anno {
  static constexpr char name[] = "anno";
  static constexpr OGRFieldType ogr_type = OFTString;
  static constexpr base::SmtVarType smt_type = base::SmtString;
};

struct field_color {
  static constexpr char name[] = "color";
  static constexpr OGRFieldType ogr_type = OFTInteger;
  static constexpr base::SmtVarType smt_type = base::SmtInteger;
};

struct field_angle {
  static constexpr char name[] = "angle";
  static constexpr OGRFieldType ogr_type = OFTReal;
  static constexpr base::SmtVarType smt_type = base::SmtReal;
};

struct field_length {
  static constexpr char name[] = "length";
  static constexpr OGRFieldType ogr_type = OFTReal;
  static constexpr base::SmtVarType smt_type = base::SmtReal;
};

struct field_area {
  static constexpr char name[] = "area";
  static constexpr OGRFieldType ogr_type = OFTReal;
  static constexpr base::SmtVarType smt_type = base::SmtReal;
};

struct field_grid_row {
  static constexpr char name[] = "grid_row";
  static constexpr OGRFieldType ogr_type = OFTInteger;
  static constexpr base::SmtVarType smt_type = base::SmtInteger;
};

struct field_grid_col {
  static constexpr char name[] = "grid_col";
  static constexpr OGRFieldType ogr_type = OFTInteger;
  static constexpr base::SmtVarType smt_type = base::SmtInteger;
};

// Marks a MultiPolygon layer as a triangle mesh. GPKG has no stable TIN type
// (gpkg_geom_TIN is non-standard and aborts in debug GDAL down_cast).
struct field_tin {
  static constexpr char name[] = "tin";
  static constexpr OGRFieldType ogr_type = OFTInteger;
  static constexpr base::SmtVarType smt_type = base::SmtInteger;
};

template <gis::SmtFeatureType Ft>
struct feature_kind_traits;

template <>
struct feature_kind_traits<gis::SmtFtDot> {
  static constexpr gis::SmtFeatureType feature_type = gis::SmtFtDot;
  static constexpr OGRwkbGeometryType wkb = wkbPoint;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<gis::SmtFtCurve> {
  static constexpr gis::SmtFeatureType feature_type = gis::SmtFtCurve;
  static constexpr OGRwkbGeometryType wkb = wkbLineString;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_length>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<gis::SmtFtSurface> {
  static constexpr gis::SmtFeatureType feature_type = gis::SmtFtSurface;
  static constexpr OGRwkbGeometryType wkb = wkbPolygon;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_area>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<gis::SmtFtAnno> {
  static constexpr gis::SmtFeatureType feature_type = gis::SmtFtAnno;
  static constexpr OGRwkbGeometryType wkb = wkbPoint;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_anno, field_color, field_angle>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<gis::SmtFtTin> {
  static constexpr gis::SmtFeatureType feature_type = gis::SmtFtTin;
  static constexpr OGRwkbGeometryType wkb = wkbMultiPolygon;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_tin>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<gis::SmtFtGrid> {
  static constexpr gis::SmtFeatureType feature_type = gis::SmtFtGrid;
  static constexpr OGRwkbGeometryType wkb = wkbMultiPoint;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_grid_row, field_grid_col>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<gis::SmtFtChildImage> {
  static constexpr gis::SmtFeatureType feature_type = gis::SmtFtChildImage;
  static constexpr OGRwkbGeometryType wkb = wkbNone;
  static constexpr bool is_raster = true;
  using extra_fields = std::tuple<>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <typename Fn>
bool visit_feature_kind(gis::SmtFeatureType ft, Fn&& fn) {
  switch (ft) {
    case gis::SmtFtDot:
      return fn(feature_kind_traits<gis::SmtFtDot>{});
    case gis::SmtFtCurve:
      return fn(feature_kind_traits<gis::SmtFtCurve>{});
    case gis::SmtFtSurface:
      return fn(feature_kind_traits<gis::SmtFtSurface>{});
    case gis::SmtFtAnno:
      return fn(feature_kind_traits<gis::SmtFtAnno>{});
    case gis::SmtFtTin:
      return fn(feature_kind_traits<gis::SmtFtTin>{});
    case gis::SmtFtGrid:
      return fn(feature_kind_traits<gis::SmtFtGrid>{});
    case gis::SmtFtChildImage:
      return fn(feature_kind_traits<gis::SmtFtChildImage>{});
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

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_OGR_FEATURE_KIND_H_
