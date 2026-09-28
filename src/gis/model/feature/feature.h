// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_MODEL_FEATURE_FEATURE_H_
#define GIS_MODEL_FEATURE_FEATURE_H_

#include "gis/gis_export.h"

class OGRFeature;
class OGRGeometry;
class OGRLayer;

namespace geo {
class Grid;
class Tin;
}  // namespace geo

namespace base {
class SmtStyle;
}

namespace render {
class SmtMaterial;
}

namespace gis {

// Product feature kind. Storage is OGRFeature; this labels extra fields /
// Grid / Tin / anno semantics.
enum SmtFeatureType {
  SmtFtDot,
  SmtFtAnno,
  SmtFtChildImage,
  SmtFtCurve,
  SmtFtSurface,
  SmtFtGrid,
  SmtFtTin,
  SmtFtUnknown
};

// Composes OGRFeature (fields + geometry) plus style / Grid / Tin / material
// sidecars. Not a second attribute store.
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
  SmtFeatureType feature_type() const { return type_; }
  void set_feature_type(SmtFeatureType type) { type_ = type; }

  OGRGeometry* geometry();
  const OGRGeometry* geometry() const;
  void set_geometry(OGRGeometry* geom);
  void set_geometry_directly(OGRGeometry* geom);
  void set_geometry(geo::Grid* grid);
  void set_geometry(geo::Tin* tin);

  base::SmtStyle* style() { return style_; }
  const base::SmtStyle* style() const { return style_; }
  void set_style(base::SmtStyle* style);
  void set_style(const char* style_name);

  geo::Grid* grid() { return grid_; }
  const geo::Grid* grid() const { return grid_; }
  geo::Tin* tin() { return tin_; }
  const geo::Tin* tin() const { return tin_; }
  void set_grid(geo::Grid* grid, bool take_ownership);
  void set_tin(geo::Tin* tin, bool take_ownership);

  render::SmtMaterial* material() { return material_; }
  void set_material(render::SmtMaterial* material, bool take_ownership);

  int field_index(const char* name) const;
  int set_field(int index, int value);
  int set_field(int index, double value);
  int set_field(int index, const char* value);

  // Leftover plugin / MFC names. They forward to the methods above.
  long GetID() const { return id(); }
  void SetID(long feature_id) { set_id(feature_id); }
  SmtFeatureType GetFeatureType() const { return feature_type(); }
  void SetFeatureType(SmtFeatureType type) { set_feature_type(type); }
  OGRGeometry* GetGeometryRef() { return geometry(); }
  const OGRGeometry* GetGeometryRef() const { return geometry(); }
  OGRGeometry* getGeometryRef() { return geometry(); }
  void SetGeometryDirectly(OGRGeometry* geom) { set_geometry_directly(geom); }
  void SetGeometry(OGRGeometry* geom) { set_geometry(geom); }
  void SetGeometry(geo::Grid* grid) { set_geometry(grid); }
  void SetGeometry(geo::Tin* tin) { set_geometry(tin); }
  void SetStyle(base::SmtStyle* style) { set_style(style); }
  void SetStyle(const char* style_name) { set_style(style_name); }
  int GetFieldIndexByName(const char* name) { return field_index(name); }
  int SetFieldValue(int index, int value) { return set_field(index, value); }
  int SetFieldValue(int index, double value) {
    return set_field(index, value);
  }
  int SetFieldValue(int index, const char* value) {
    return set_field(index, value);
  }

 private:
  void clear_sidecars();
  void take_from(Feature&& other) noexcept;

  OGRFeature* ogr_ = nullptr;
  bool owns_ogr_ = true;
  SmtFeatureType type_ = SmtFtUnknown;
  base::SmtStyle* style_ = nullptr;
  geo::Grid* grid_ = nullptr;
  bool owns_grid_ = false;
  geo::Tin* tin_ = nullptr;
  bool owns_tin_ = false;
  render::SmtMaterial* material_ = nullptr;
  bool owns_material_ = false;
};

// Leftover TUs still name the product type SmtFeature.
using SmtFeature = Feature;

bool GIS_EXPORT leftover_append_feature(OGRLayer* layer, Feature* feature);

}  // namespace gis

#endif  // GIS_MODEL_FEATURE_FEATURE_H_
