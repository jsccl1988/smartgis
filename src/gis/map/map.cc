// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/map/map.h"

#include <cstring>
#include <string>
#include <utility>

#include "gis/feature/feature.h"
#include "gis/geo/ops/geometry_traits.h"
#include "ogrsf_frmts.h"

namespace gis {

Map::Map() : active_(-1), iterator_index_(0) {
  strcpy_s(name_, k_map_name_max, "DefMap");
  envelope_.MinX = 0;
  envelope_.MinY = 0;
  envelope_.MaxX = 800;
  envelope_.MaxY = 600;
}

Map::~Map() { clear(); }

int Map::index_of_name(const char* name) const {
  if (!name) {
    return -1;
  }
  for (int i = 0; i < static_cast<int>(layers_.size()); ++i) {
    const char* layer = layer_name(i);
    if (layer && std::strcmp(layer, name) == 0) {
      return i;
    }
  }
  return -1;
}

const char* Map::layer_name(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return "";
  }
  return layers_[index].name();
}

LayerType Map::layer_type(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return LayerType::kVector;
  }
  return layers_[index].layer_type();
}

bool Map::is_layer_visible(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return false;
  }
  return layers_[index].visible();
}

void Map::set_layer_visible(int index, bool visible) {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return;
  }
  layers_[index].set_visible(visible);
}

void Map::envelope_of(const MapLayer& layer, Envelope* env) const {
  layer.get_envelope(env);
}

bool Map::add_layer(MapLayer layer) {
  if ((!layer.ogr() && !layer.raster() && !layer.tile()) ||
      index_of_name(layer.name()) >= 0) {
    return false;
  }
  if (layers_.empty()) {
    envelope_.MaxX = envelope_.MinX = envelope_.MaxY = envelope_.MinY =
        k_envelope_unset;
  }
  Envelope lyr;
  layer.get_envelope(&lyr);
  layers_.push_back(std::move(layer));
  active_ = static_cast<int>(layers_.size()) - 1;
  envelope_.merge(lyr);
  return true;
}

bool Map::add_layer(OGRLayer* layer) {
  return add_layer(MapLayer::from_ogr(layer));
}

bool Map::delete_layer(const char* name) {
  const int i = index_of_name(name);
  if (i < 0) {
    return false;
  }
  layers_.erase(layers_.begin() + i);
  if (active_ == i) {
    active_ = -1;
  } else if (active_ > i) {
    --active_;
  }
  cal_envelope();
  return true;
}

bool Map::delete_layer(OGRLayer* layer) {
  return layer && delete_layer(layer->GetName());
}

bool Map::move_to(int from_index, int to_index) {
  if (from_index < 0 || to_index < 0 ||
      from_index >= static_cast<int>(layers_.size()) ||
      to_index >= static_cast<int>(layers_.size())) {
    return false;
  }
  std::swap(layers_[from_index], layers_[to_index]);
  if (active_ == from_index) {
    active_ = to_index;
  } else if (active_ == to_index) {
    active_ = from_index;
  }
  return true;
}

bool Map::move_to_bottom(int index) {
  return move_to(index, static_cast<int>(layers_.size()) - 1);
}

bool Map::move_to_top(int index) { return move_to(index, 0); }

void Map::set_active_layer(const char* name) {
  active_ = index_of_name(name);
}

void Map::set_active_ogr_layer(OGRLayer* layer) {
  if (!layer) {
    active_ = -1;
    return;
  }
  for (int i = 0; i < static_cast<int>(layers_.size()); ++i) {
    if (layers_[i].ogr() == layer) {
      active_ = i;
      return;
    }
  }
}

MapLayer* Map::active_map_layer() { return map_layer(active_); }

const MapLayer* Map::active_map_layer() const { return map_layer(active_); }

OGRLayer* Map::active_ogr_layer() { return ogr_layer(active_); }

const OGRLayer* Map::active_ogr_layer() const { return ogr_layer(active_); }

MapLayer* Map::map_layer(const char* name) {
  return map_layer(index_of_name(name));
}

const MapLayer* Map::map_layer(const char* name) const {
  return map_layer(index_of_name(name));
}

MapLayer* Map::map_layer(int index) {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return nullptr;
  }
  return &layers_[index];
}

const MapLayer* Map::map_layer(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return nullptr;
  }
  return &layers_[index];
}

OGRLayer* Map::ogr_layer(const char* name) {
  return ogr_layer(index_of_name(name));
}

const OGRLayer* Map::ogr_layer(const char* name) const {
  return ogr_layer(index_of_name(name));
}

OGRLayer* Map::ogr_layer(int index) {
  MapLayer* layer = map_layer(index);
  return layer ? layer->ogr() : nullptr;
}

const OGRLayer* Map::ogr_layer(int index) const {
  const MapLayer* layer = map_layer(index);
  return layer ? layer->ogr() : nullptr;
}

void Map::move_first() const { iterator_index_ = 0; }

void Map::move_next() const {
  if (iterator_index_ < static_cast<int>(layers_.size())) {
    ++iterator_index_;
  }
}

void Map::move_last() const {
  iterator_index_ = static_cast<int>(layers_.size()) - 1;
}

void Map::erase() {
  if (iterator_index_ < 0 ||
      iterator_index_ >= static_cast<int>(layers_.size())) {
    return;
  }
  layers_.erase(layers_.begin() + iterator_index_);
  if (active_ == iterator_index_) {
    active_ = -1;
  } else if (active_ > iterator_index_) {
    --active_;
  }
  cal_envelope();
}

void Map::clear() {
  layers_.clear();
  active_ = -1;
}

bool Map::is_end() const {
  return iterator_index_ == static_cast<int>(layers_.size());
}

void Map::cal_envelope() {
  Envelope lyr;
  envelope_ = Envelope();
  for (MapLayer& layer : layers_) {
    layer.cal_envelope();
    envelope_of(layer, &lyr);
    envelope_.merge(lyr);
  }
}

bool Map::append_feature(OGRFeature* feature) {
  OGRLayer* lyr = active_ogr_layer();
  if (!lyr || !feature) {
    return false;
  }
  return lyr->CreateFeature(feature) == OGRERR_NONE;
}

bool Map::append_feature(Feature* feature, bool /*clone*/) {
  OGRLayer* lyr = active_ogr_layer();
  if (!lyr || !feature) {
    return false;
  }
  OGRFeature* src = feature->ogr();
  if (src && src->GetDefnRef() == lyr->GetLayerDefn()) {
    return lyr->CreateFeature(src) == OGRERR_NONE;
  }
  OGRFeature* dst = OGRFeature::CreateFeature(lyr->GetLayerDefn());
  if (!dst) {
    return false;
  }
  if (src) {
    dst->SetFrom(src);
  } else if (feature->geometry()) {
    dst->SetGeometry(feature->geometry());
  }
  dst->SetFID(feature->id());
  const OGRErr err = lyr->CreateFeature(dst);
  OGRFeature::DestroyFeature(dst);
  return err == OGRERR_NONE;
}

bool Map::delete_feature(OGRFeature* feature) {
  OGRLayer* lyr = active_ogr_layer();
  if (!lyr || !feature) {
    return false;
  }
  return lyr->DeleteFeature(feature->GetFID()) == OGRERR_NONE;
}

bool Map::update_feature(OGRFeature* feature) {
  OGRLayer* lyr = active_ogr_layer();
  if (!lyr || !feature) {
    return false;
  }
  return lyr->SetFeature(feature) == OGRERR_NONE;
}

namespace {

int feature_type_from_geom(const OGRGeometry* geom) {
  if (!geom) {
    return static_cast<int>(wkbUnknown);
  }
  return static_cast<int>(wkbFlatten(geom->getGeometryType()));
}

void apply_query_filters(OGRLayer* lyr, const GeomQueryDesc* gquery,
                         const AttrQueryDesc* aquery, const Envelope* env,
                         bool have_env) {
  if (have_env && env) {
    lyr->SetSpatialFilterRect(env->MinX, env->MinY, env->MaxX, env->MaxY);
  } else if (gquery && gquery->geometry) {
    lyr->SetSpatialFilter(gquery->geometry);
  }
  if (aquery && aquery->field_names && aquery->field_names[0] &&
      aquery->field_queries && aquery->field_queries[0]) {
    std::string attr = aquery->field_names[0];
    attr += aquery->field_queries[0];
    if (lyr->SetAttributeFilter(attr.c_str()) != OGRERR_NONE) {
      lyr->SetAttributeFilter(nullptr);
    }
  }
}

}  // namespace

bool Map::query_feature(const GeomQueryDesc* gquery,
                        const AttrQueryDesc* aquery, OGRLayer* result,
                        int& geom_type) {
  if (!result) {
    return false;
  }
  geom_type = static_cast<int>(wkbUnknown);
  Envelope env;
  bool have_env = false;
  if (gquery && gquery->geometry) {
    geo::fill_envelope(*gquery->geometry, &env);
    double margin = gquery->margin;
    if (margin < 0) {
      margin = 0;
    }
    // A degenerate point envelope misses nearby vertices (city dots).
    if (env.MinX == env.MaxX || env.MinY == env.MaxY) {
      if (margin <= 0) {
        margin = 1e-6;
      }
    }
    if (margin > 0) {
      env.MinX -= margin;
      env.MinY -= margin;
      env.MaxX += margin;
      env.MaxY += margin;
    }
    have_env = true;
  }

  bool queried = false;
  for (int i = 0; i < layer_count(); ++i) {
    if (!is_layer_visible(i)) {
      continue;
    }
    OGRLayer* lyr = ogr_layer(i);
    if (!lyr) {
      continue;
    }
    queried = true;
    apply_query_filters(lyr, gquery, aquery, &env, have_env);
    lyr->ResetReading();
    while (OGRFeature* feat = lyr->GetNextFeature()) {
      append_cloned_feature(result, feat);
      if (geom_type == static_cast<int>(wkbUnknown)) {
        geom_type = feature_type_from_geom(feat->GetGeometryRef());
      }
      OGRFeature::DestroyFeature(feat);
    }
    lyr->SetSpatialFilter(nullptr);
    lyr->SetAttributeFilter(nullptr);
  }
  return queried;
}

}  // namespace gis
