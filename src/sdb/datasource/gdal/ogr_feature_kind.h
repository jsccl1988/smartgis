// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_FEATURE_KIND_H_
#define SDB_DATASOURCE_GDAL_OGR_FEATURE_KIND_H_

#include "sdb/feature/feature.h"

#include "ogr_core.h"

#include <tuple>
#include <utility>

class OGRFeature;

class OGRGeometry;

namespace sdb {
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

template <sdb::SmtFeatureType Ft>
struct feature_kind_traits;

template <>
struct feature_kind_traits<sdb::SmtFtDot> {
  static constexpr sdb::SmtFeatureType feature_type = sdb::SmtFtDot;
  static constexpr OGRwkbGeometryType wkb = wkbPoint;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<sdb::SmtFtCurve> {
  static constexpr sdb::SmtFeatureType feature_type = sdb::SmtFtCurve;
  static constexpr OGRwkbGeometryType wkb = wkbLineString;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_length>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<sdb::SmtFtSurface> {
  static constexpr sdb::SmtFeatureType feature_type = sdb::SmtFtSurface;
  static constexpr OGRwkbGeometryType wkb = wkbPolygon;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_area>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<sdb::SmtFtAnno> {
  static constexpr sdb::SmtFeatureType feature_type = sdb::SmtFtAnno;
  static constexpr OGRwkbGeometryType wkb = wkbPoint;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_anno, field_color, field_angle>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<sdb::SmtFtTin> {
  static constexpr sdb::SmtFeatureType feature_type = sdb::SmtFtTin;
  static constexpr OGRwkbGeometryType wkb = wkbTIN;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<sdb::SmtFtGrid> {
  static constexpr sdb::SmtFeatureType feature_type = sdb::SmtFtGrid;
  static constexpr OGRwkbGeometryType wkb = wkbMultiPoint;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_grid_row, field_grid_col>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <>
struct feature_kind_traits<sdb::SmtFtChildImage> {
  static constexpr sdb::SmtFeatureType feature_type = sdb::SmtFtChildImage;
  static constexpr OGRwkbGeometryType wkb = wkbNone;
  static constexpr bool is_raster = true;
  using extra_fields = std::tuple<>;
  static bool encode_geom(const OGRGeometry* src, OGRFeature* dst);
  static OGRGeometry* decode_geom(OGRFeature* src);
};

template <typename Fn>
bool visit_feature_kind(sdb::SmtFeatureType ft, Fn&& fn) {
  switch (ft) {
    case sdb::SmtFtDot:
      return fn(feature_kind_traits<sdb::SmtFtDot>{});
    case sdb::SmtFtCurve:
      return fn(feature_kind_traits<sdb::SmtFtCurve>{});
    case sdb::SmtFtSurface:
      return fn(feature_kind_traits<sdb::SmtFtSurface>{});
    case sdb::SmtFtAnno:
      return fn(feature_kind_traits<sdb::SmtFtAnno>{});
    case sdb::SmtFtTin:
      return fn(feature_kind_traits<sdb::SmtFtTin>{});
    case sdb::SmtFtGrid:
      return fn(feature_kind_traits<sdb::SmtFtGrid>{});
    case sdb::SmtFtChildImage:
      return fn(feature_kind_traits<sdb::SmtFtChildImage>{});
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
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_FEATURE_KIND_H_
