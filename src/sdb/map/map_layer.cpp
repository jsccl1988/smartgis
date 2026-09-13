// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/map/map_layer.h"

#include "base/core/core.h"

#include "ogrsf_frmts.h"

namespace sdb {

MapLayer::~MapLayer() {
  if (owns_leftover_) {
    SMT_SAFE_DELETE(leftover_);
  }
  leftover_ = nullptr;
  if (owned_ds_) {
    GDALClose(owned_ds_);
    owned_ds_ = nullptr;
    ogr_ = nullptr;
    owns_ogr_ = false;
  }
  ogr_ = nullptr;
}

MapLayer::MapLayer(MapLayer&& other) noexcept
    : type_(other.type_),
      ogr_(other.ogr_),
      owns_ogr_(other.owns_ogr_),
      owned_ds_(other.owned_ds_),
      leftover_(other.leftover_),
      owns_leftover_(other.owns_leftover_),
      visible_(other.visible_),
      feature_type_(other.feature_type_),
      style_name_(std::move(other.style_name_)) {
  other.ogr_ = nullptr;
  other.owns_ogr_ = false;
  other.owned_ds_ = nullptr;
  other.leftover_ = nullptr;
  other.owns_leftover_ = false;
}

MapLayer& MapLayer::operator=(MapLayer&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (owns_leftover_) {
    SMT_SAFE_DELETE(leftover_);
  }
  if (owned_ds_) {
    GDALClose(owned_ds_);
  }
  type_ = other.type_;
  ogr_ = other.ogr_;
  owns_ogr_ = other.owns_ogr_;
  owned_ds_ = other.owned_ds_;
  leftover_ = other.leftover_;
  owns_leftover_ = other.owns_leftover_;
  visible_ = other.visible_;
  feature_type_ = other.feature_type_;
  style_name_ = std::move(other.style_name_);
  other.ogr_ = nullptr;
  other.owns_ogr_ = false;
  other.owned_ds_ = nullptr;
  other.leftover_ = nullptr;
  other.owns_leftover_ = false;
  return *this;
}

MapLayer MapLayer::from_ogr(OGRLayer* layer) {
  MapLayer m;
  m.type_ = LYR_VECTOR;
  m.ogr_ = layer;
  m.owns_ogr_ = false;
  return m;
}

MapLayer MapLayer::adopt_dataset(GDALDataset* ds, OGRLayer* layer) {
  MapLayer m;
  m.type_ = LYR_VECTOR;
  m.owned_ds_ = ds;
  m.ogr_ = layer;
  m.owns_ogr_ = ds != nullptr;
  return m;
}

MapLayer MapLayer::from_leftover(SmtLayer* layer, bool owns) {
  MapLayer m;
  m.leftover_ = layer;
  m.owns_leftover_ = owns && layer != nullptr;
  m.type_ = layer ? layer->GetLayerType() : LYR_VECTOR;
  m.visible_ = layer ? layer->IsVisible() : true;
  return m;
}

const char* MapLayer::name() const {
  if (ogr_) {
    return ogr_->GetName();
  }
  if (leftover_) {
    return leftover_->GetLayerName();
  }
  return "";
}

bool MapLayer::visible() const {
  if (leftover_) {
    return leftover_->IsVisible();
  }
  return visible_;
}

void MapLayer::set_visible(bool visible) {
  visible_ = visible;
  if (leftover_) {
    leftover_->SetVisible(visible);
  }
}

void MapLayer::set_style_name(const char* name) {
  style_name_ = name ? name : "";
}

void MapLayer::get_envelope(base::Envelope* out) const {
  if (!out) {
    return;
  }
  *out = base::Envelope();
  if (leftover_) {
    leftover_->get_envelope(*out);
    return;
  }
  if (!ogr_) {
    return;
  }
  OGREnvelope ogr_env;
  if (ogr_->GetExtent(&ogr_env, TRUE) == OGRERR_NONE) {
    out->MinX = ogr_env.MinX;
    out->MinY = ogr_env.MinY;
    out->MaxX = ogr_env.MaxX;
    out->MaxY = ogr_env.MaxY;
  }
}

void MapLayer::cal_envelope() {
  if (leftover_) {
    leftover_->CalEnvelope();
  }
}

}  // namespace sdb
