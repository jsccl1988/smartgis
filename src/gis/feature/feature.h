// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_FEATURE_FEATURE_H_
#define GIS_FEATURE_FEATURE_H_

#include "gis/gis_export.h"
#include "ogr_core.h"

class OGRFeature;
class OGRGeometry;
class OGRLayer;

namespace gis {

// Composes one OGRFeature (fields + geometry). Geometry classification is
// OGRwkbGeometryType on the instance (Z/M/25D live on that type). Leftover
// carto kinds (Anno / ChildImage / Grid / Tin catalog) are not stored here.
class GIS_EXPORT Feature {
 public:
  Feature();
  Feature(OGRFeature* ogr, bool take_ownership);
  ~Feature();

  Feature(Feature&& other) noexcept;
  Feature& operator=(Feature&& other) noexcept;
  Feature(const Feature&) = delete;
  Feature& operator=(const Feature&) = delete;

  static Feature borrow(OGRFeature* ogr);
  OGRFeature* release();
  void reset_ogr(OGRFeature* ogr, bool take_ownership);

  OGRFeature* ogr() { return ogr_; }
  const OGRFeature* ogr() const { return ogr_; }
  bool owns_ogr() const { return owns_ogr_; }

  long id() const;
  void set_id(long id);

  // Raw OGR type of the owned geometry (not flattened). wkbNone if no geom.
  OGRwkbGeometryType geometry_type() const;

  OGRGeometry* geometry();
  const OGRGeometry* geometry() const;
  void set_geometry(OGRGeometry* geom);
  void set_geometry_directly(OGRGeometry* geom);

  int field_index(const char* name) const;
  int field_count() const;
  OGRFieldType field_type(int index) const;
  bool is_field_set(int index) const;
  bool is_field_null(int index) const;

  // Field I/O is OGRFeature only. set_field returns 0 on success, 1 on failure.
  int get_field_as_integer(int index) const;
  GIntBig get_field_as_integer64(int index) const;
  double get_field_as_double(int index) const;
  const char* get_field_as_string(int index) const;
  const int* get_field_as_integer_list(int index, int* count) const;
  const GIntBig* get_field_as_integer64_list(int index, int* count) const;
  const double* get_field_as_real_list(int index, int* count) const;
  char** get_field_as_string_list(int index) const;
  GByte* get_field_as_binary(int index, int* byte_count) const;
  bool get_field_as_date_time(int index, int* year, int* month, int* day,
                              int* hour, int* minute, float* second,
                              int* tzflag) const;

  int set_field(int index, int value);
  int set_field(int index, GIntBig value);
  int set_field(int index, double value);
  int set_field(int index, const char* value);
  int set_field_integer_list(int index, int count, const int* values);
  int set_field_integer64_list(int index, int count, const GIntBig* values);
  int set_field_real_list(int index, int count, const double* values);
  int set_field_string_list(int index, CSLConstList values);
  int set_field_binary(int index, int byte_count, const void* data);
  int set_field_date_time(int index, int year, int month, int day, int hour,
                          int minute, float second, int tzflag);

 private:
  void take_from(Feature&& other) noexcept;

  OGRFeature* ogr_ = nullptr;
  bool owns_ogr_ = true;
};

// Clone src onto dest's feature defn, then CreateFeature. 0 = ok, 1 = fail.
GIS_EXPORT long append_cloned_feature(OGRLayer* dest, const OGRFeature* src);
// Copy every feature from src onto dest. 0 = ok, 1 = fail.
GIS_EXPORT long copy_ogr_layer(OGRLayer* dest, OGRLayer* src);

}  // namespace gis

#endif  // GIS_FEATURE_FEATURE_H_
