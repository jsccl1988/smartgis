// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_FEATURE_FEATURE_H_
#define SDB_FEATURE_FEATURE_H_

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
// sidecars. Not a second attribute store. Class name stays SmtFeature for DLL
// ABI.
class GIS_EXPORT SmtFeature {
 public:
  SmtFeature();
  SmtFeature(OGRFeature* ogr, bool take_ownership);
  ~SmtFeature();

  SmtFeature(SmtFeature&& other) noexcept;
  SmtFeature& operator=(SmtFeature&& other) noexcept;
  SmtFeature(const SmtFeature&) = delete;
  SmtFeature& operator=(const SmtFeature&) = delete;

  static SmtFeature borrow(OGRFeature* ogr);
  OGRFeature* release();
  void reset_ogr(OGRFeature* ogr, bool take_ownership);

  OGRFeature* ogr() { return ogr_; }
  const OGRFeature* ogr() const { return ogr_; }
  bool owns_ogr() const { return owns_ogr_; }

  long id() const;
  void set_id(long id);
  SmtFeatureType feature_type() const { return type_; }
  void set_feature_type(SmtFeatureType type);

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

  // Out-of-line PascalCase API for leftover plugin / MFC TU ABI.
  long GetID() const;
  void SetID(long id);
  SmtFeatureType GetFeatureType() const;
  void SetFeatureType(SmtFeatureType type);
  OGRGeometry* GetGeometryRef();
  const OGRGeometry* GetGeometryRef() const;
  OGRGeometry* getGeometryRef();
  void SetGeometryDirectly(OGRGeometry* geom);
  void SetGeometry(OGRGeometry* geom);
  void SetGeometry(geo::Grid* g);
  void SetGeometry(geo::Tin* t);
  base::SmtStyle* get_style();
  void SetStyle(base::SmtStyle* s);
  void SetStyle(const char* name);
  int GetFieldIndexByName(const char* name);
  int SetFieldValue(int index, int v);
  int SetFieldValue(int index, double v);
  int SetFieldValue(int index, const char* v);

 private:
  void clear_sidecars();

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

bool GIS_EXPORT leftover_append_feature(OGRLayer* layer, SmtFeature* feature);

}  // namespace gis

#endif  // SDB_FEATURE_FEATURE_H_
