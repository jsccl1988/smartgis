// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_FEATURE_LEFTOVER_FEATURE_H_
#define SMT_LEGACY_GIS_FEATURE_LEFTOVER_FEATURE_H_

#include "gis/datasource/ogr/ogr_feature_codec.h"
#include "gis/feature/feature.h"
#include "gis/map/layer_kind.h"
#include "legacy/gis/layer/layer.h"
#include "legacy/gis/present/carto/stylemanager.h"

class OGRLayer;

// Leftover PascalCase adapter over gis::Feature. Owns leftover style/material
// sidecars and leftover catalog FeatureType (Anno/Grid/Tin/…).

namespace base {
class SmtStyle;
}
namespace render {
class SmtMaterial;
}

namespace gis {

inline VectorSchema leftover_vector_schema(FeatureType ft) {
  switch (ft) {
    case FtAnno:
      return VectorSchema::kAnno;
    case FtChildImage:
      return VectorSchema::kChildImage;
    case FtGrid:
      return VectorSchema::kGrid;
    case FtTin:
      return VectorSchema::kTin;
    default:
      return VectorSchema::kNone;
  }
}

inline FeatureType leftover_feature_type_of(OGRFeature* feat) {
  if (!feat) {
    return FtUnknown;
  }
  const VectorSchema schema = datasource::infer_vector_schema(feat);
  switch (schema) {
    case VectorSchema::kAnno:
      return FtAnno;
    case VectorSchema::kChildImage:
      return FtChildImage;
    case VectorSchema::kGrid:
      return FtGrid;
    case VectorSchema::kTin:
      return FtTin;
    default:
      break;
  }
  switch (wkbFlatten(datasource::infer_geometry_type(feat))) {
    case wkbPoint:
    case wkbMultiPoint:
      return FtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return FtCurve;
    case wkbPolygon:
    case wkbLinearRing:
    case wkbMultiPolygon:
      return FtSurface;
    case wkbTIN:
    case wkbTriangle:
      return FtTin;
    default:
      return FtUnknown;
  }
}

inline FeatureType leftover_feature_type_from_wkb(int wkb) {
  switch (wkbFlatten(static_cast<OGRwkbGeometryType>(wkb))) {
    case wkbPoint:
    case wkbMultiPoint:
      return FtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return FtCurve;
    case wkbPolygon:
    case wkbLinearRing:
    case wkbMultiPolygon:
      return FtSurface;
    case wkbTIN:
    case wkbTriangle:
      return FtTin;
    default:
      return FtUnknown;
  }
}

inline bool leftover_encode_geometry(const OGRGeometry* src, OGRFeature* dst,
                                     FeatureType ft) {
  return datasource::encode_ogr_geometry(src, dst, leftover_feature_wkb(ft),
                                         leftover_vector_schema(ft));
}

inline OGRGeometry* leftover_decode_geometry(OGRFeature* src, FeatureType hint) {
  return datasource::decode_ogr_geometry(src, leftover_vector_schema(hint));
}

class FeatureAdapter : public Feature {
 public:
  FeatureAdapter() = default;
  FeatureAdapter(OGRFeature* ogr, bool take_ownership)
      : Feature(ogr, take_ownership) {}
  FeatureAdapter(FeatureAdapter&&) noexcept = default;
  FeatureAdapter& operator=(FeatureAdapter&&) noexcept = default;
  ~FeatureAdapter() {
    if (owns_material_) {
      ::operator delete(material_);
    }
  }

  long GetID() const { return id(); }
  void SetID(long feature_id) { set_id(feature_id); }
  FeatureType GetFeatureType() const { return leftover_type_; }
  void SetFeatureType(FeatureType type) { leftover_type_ = type; }
  OGRGeometry* GetGeometryRef() { return geometry(); }
  const OGRGeometry* GetGeometryRef() const { return geometry(); }
  OGRGeometry* getGeometryRef() { return geometry(); }
  void SetGeometryDirectly(OGRGeometry* geom) { set_geometry_directly(geom); }
  void SetGeometry(OGRGeometry* geom) { set_geometry(geom); }
  void SetStyle(base::SmtStyle* style) { style_ = style; }
  void SetStyle(const char* style_name) {
    base::SmtStyleManager* mgr = base::SmtStyleManager::get_singleton_ptr();
    style_ = (mgr && style_name) ? mgr->get_style(style_name) : nullptr;
  }
  base::SmtStyle* GetStyle() { return style_; }
  const base::SmtStyle* GetStyle() const { return style_; }
  void SetMaterial(render::SmtMaterial* material, bool take_ownership) {
    if (owns_material_ && material_ != material) {
      ::operator delete(material_);
    }
    material_ = material;
    owns_material_ = take_ownership && material != nullptr;
  }
  render::SmtMaterial* GetMaterial() { return material_; }
  int GetFieldIndexByName(const char* name) { return field_index(name); }
  int SetFieldValue(int index, int value) { return set_field(index, value); }
  int SetFieldValue(int index, double value) {
    return set_field(index, value);
  }
  int SetFieldValue(int index, const char* value) {
    return set_field(index, value);
  }

 private:
  FeatureType leftover_type_ = FtUnknown;
  base::SmtStyle* style_ = nullptr;
  render::SmtMaterial* material_ = nullptr;
  bool owns_material_ = false;
};

inline long leftover_feature_id(const Feature& feature) { return feature.id(); }
inline void leftover_feature_set_id(Feature* feature, long id) {
  if (feature) {
    feature->set_id(id);
  }
}

bool GIS_EXPORT leftover_append_feature(OGRLayer* layer, Feature* feature);

}  // namespace gis

#endif  // SMT_LEGACY_GIS_FEATURE_LEFTOVER_FEATURE_H_
