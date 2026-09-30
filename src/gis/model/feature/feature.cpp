// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/model/feature/feature.h"

#include <utility>

#include "legacy/core/macros/macros.h"
#include "ogrsf_frmts.h"
#include "legacy/gis/present/carto/stylemanager.h"

namespace gis {
namespace {

// SmtMaterial is defined in leftover render3d; keep this TU free of that DLL.
// Type is trivially destructible (POD color fields) — storage-only free.
void destroy_opaque_material(render::SmtMaterial* material) {
  ::operator delete(material);
}

}  // namespace

Feature::Feature() = default;

Feature::Feature(OGRFeature* ogr, bool take_ownership)
    : ogr_(ogr), owns_ogr_(take_ownership && ogr != nullptr) {}

Feature::~Feature() {
  clear_sidecars();
  if (owns_ogr_ && ogr_) {
    OGRFeature::DestroyFeature(ogr_);
    ogr_ = nullptr;
  }
}

Feature::Feature(Feature&& other) noexcept { take_from(std::move(other)); }

Feature& Feature::operator=(Feature&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  clear_sidecars();
  if (owns_ogr_ && ogr_) {
    OGRFeature::DestroyFeature(ogr_);
  }
  take_from(std::move(other));
  return *this;
}

Feature Feature::borrow(OGRFeature* ogr) { return Feature(ogr, false); }

OGRFeature* Feature::release() {
  OGRFeature* out = ogr_;
  ogr_ = nullptr;
  owns_ogr_ = false;
  return out;
}

void Feature::reset_ogr(OGRFeature* ogr, bool take_ownership) {
  if (owns_ogr_ && ogr_ && ogr_ != ogr) {
    OGRFeature::DestroyFeature(ogr_);
  }
  ogr_ = ogr;
  owns_ogr_ = take_ownership && ogr != nullptr;
}

void Feature::take_from(Feature&& other) noexcept {
  ogr_ = other.ogr_;
  owns_ogr_ = other.owns_ogr_;
  type_ = other.type_;
  style_ = other.style_;
  grid_ = other.grid_;
  owns_grid_ = other.owns_grid_;
  tin_ = other.tin_;
  owns_tin_ = other.owns_tin_;
  material_ = other.material_;
  owns_material_ = other.owns_material_;
  other.ogr_ = nullptr;
  other.owns_ogr_ = false;
  other.type_ = SmtFtUnknown;
  other.style_ = nullptr;
  other.grid_ = nullptr;
  other.owns_grid_ = false;
  other.tin_ = nullptr;
  other.owns_tin_ = false;
  other.material_ = nullptr;
  other.owns_material_ = false;
}

void Feature::clear_sidecars() {
  if (owns_grid_) {
    SMT_SAFE_DELETE(grid_);
  } else {
    grid_ = nullptr;
  }
  owns_grid_ = false;
  if (owns_tin_) {
    SMT_SAFE_DELETE(tin_);
  } else {
    tin_ = nullptr;
  }
  owns_tin_ = false;
  if (owns_material_ && material_) {
    destroy_opaque_material(material_);
    material_ = nullptr;
  } else {
    material_ = nullptr;
  }
  owns_material_ = false;
  style_ = nullptr;
}

long Feature::id() const {
  return ogr_ ? static_cast<long>(ogr_->GetFID()) : 0;
}

void Feature::set_id(long id) {
  if (ogr_) {
    ogr_->SetFID(id);
  }
}

OGRGeometry* Feature::geometry() {
  return ogr_ ? ogr_->GetGeometryRef() : nullptr;
}

const OGRGeometry* Feature::geometry() const {
  return ogr_ ? ogr_->GetGeometryRef() : nullptr;
}

void Feature::set_geometry(OGRGeometry* geom) {
  if (ogr_) {
    ogr_->SetGeometry(geom);
  }
}

void Feature::set_geometry_directly(OGRGeometry* geom) {
  if (ogr_) {
    ogr_->SetGeometryDirectly(geom);
  }
}

void Feature::set_geometry(geo::Grid* grid) { set_grid(grid, false); }

void Feature::set_geometry(geo::Tin* tin) { set_tin(tin, false); }

void Feature::set_style(base::SmtStyle* style) { style_ = style; }

void Feature::set_style(const char* style_name) {
  base::SmtStyleManager* mgr = base::SmtStyleManager::get_singleton_ptr();
  style_ = (mgr && style_name) ? mgr->get_style(style_name) : nullptr;
}

void Feature::set_grid(geo::Grid* grid, bool take_ownership) {
  if (owns_grid_ && grid_ != grid) {
    SMT_SAFE_DELETE(grid_);
  }
  grid_ = grid;
  owns_grid_ = take_ownership && grid != nullptr;
}

void Feature::set_tin(geo::Tin* tin, bool take_ownership) {
  if (owns_tin_ && tin_ != tin) {
    SMT_SAFE_DELETE(tin_);
  }
  tin_ = tin;
  owns_tin_ = take_ownership && tin != nullptr;
}

void Feature::set_material(render::SmtMaterial* material, bool take_ownership) {
  if (owns_material_ && material_ != material && material_) {
    destroy_opaque_material(material_);
    material_ = nullptr;
  }
  material_ = material;
  owns_material_ = take_ownership && material != nullptr;
}

int Feature::field_index(const char* name) const {
  if (!ogr_ || !name) {
    return -1;
  }
  return ogr_->GetFieldIndex(name);
}

int Feature::set_field(int index, int value) {
  if (!ogr_ || index < 0) {
    return SMT_ERR_FAILURE;
  }
  ogr_->SetField(index, value);
  return SMT_ERR_NONE;
}

int Feature::set_field(int index, double value) {
  if (!ogr_ || index < 0) {
    return SMT_ERR_FAILURE;
  }
  ogr_->SetField(index, value);
  return SMT_ERR_NONE;
}

int Feature::set_field(int index, const char* value) {
  if (!ogr_ || index < 0) {
    return SMT_ERR_FAILURE;
  }
  ogr_->SetField(index, value);
  return SMT_ERR_NONE;
}

bool GIS_EXPORT leftover_append_feature(OGRLayer* layer, Feature* feature) {
  if (!layer || !feature) {
    return false;
  }
  OGRFeature* src = feature->ogr();
  if (src && src->GetDefnRef() == layer->GetLayerDefn()) {
    return layer->CreateFeature(src) == OGRERR_NONE;
  }
  OGRFeature* dst = OGRFeature::CreateFeature(layer->GetLayerDefn());
  if (!dst) {
    return false;
  }
  if (src) {
    dst->SetFrom(src);
  } else if (feature->geometry()) {
    dst->SetGeometry(feature->geometry());
  }
  dst->SetFID(feature->id());
  const OGRErr err = layer->CreateFeature(dst);
  OGRFeature::DestroyFeature(dst);
  return err == OGRERR_NONE;
}

}  // namespace gis
