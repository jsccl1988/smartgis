// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/store/layer_store.h"

#include <algorithm>
#include <cstring>

namespace content {
namespace detail {

void LayerStore::clear() {
  layers_.clear();
  active_layer_id_.clear();
  selected_id_ = {};
  last_open_was_ogr_ = false;
}

MapLayer* LayerStore::find_layer(const std::string& id) {
  for (MapLayer& layer : layers_) {
    if (layer.id == id) {
      return &layer;
    }
  }
  return nullptr;
}

const MapLayer* LayerStore::find_layer(const std::string& id) const {
  for (const MapLayer& layer : layers_) {
    if (layer.id == id) {
      return &layer;
    }
  }
  return nullptr;
}

MapFeature* LayerStore::find_feature(const content::FeatureId& id) {
  for (MapLayer& layer : layers_) {
    for (MapFeature& f : layer.features) {
      if (feature_id_eq(f.id, id)) {
        return &f;
      }
    }
  }
  return nullptr;
}

const MapFeature* LayerStore::find_feature(
    const content::FeatureId& id) const {
  for (const MapLayer& layer : layers_) {
    for (const MapFeature& f : layer.features) {
      if (feature_id_eq(f.id, id)) {
        return &f;
      }
    }
  }
  return nullptr;
}

content::FeatureId LayerStore::next_feature_id() {
  content::FeatureId id{};
  id.len = 4;
  const uint32_t v = next_id_++;
  id.bytes[0] = static_cast<uint8_t>(v & 0xff);
  id.bytes[1] = static_cast<uint8_t>((v >> 8) & 0xff);
  id.bytes[2] = static_cast<uint8_t>((v >> 16) & 0xff);
  id.bytes[3] = static_cast<uint8_t>((v >> 24) & 0xff);
  return id;
}

void LayerStore::ensure_active_layer_or_front() {
  if (find_layer(active_layer_id_)) {
    return;
  }
  if (layers_.empty()) {
    return;
  }
  active_layer_id_ = layers_.front().id;
}

std::vector<content::LayerDesc> LayerStore::layer_descs() const {
  std::vector<content::LayerDesc> out;
  out.reserve(layers_.size());
  for (const MapLayer& layer : layers_) {
    content::LayerDesc d;
    d.id = layer.id;
    d.name = layer.name;
    d.visible = layer.visible;
    d.active = (layer.id == active_layer_id_);
    out.push_back(std::move(d));
  }
  return out;
}

size_t LayerStore::feature_count() const {
  size_t n = 0;
  for (const MapLayer& layer : layers_) {
    n += layer.features.size();
  }
  return n;
}

bool LayerStore::create_layer(const std::string& name) {
  if (name.empty()) {
    return false;
  }
  std::string id = name;
  int suffix = 1;
  while (find_layer(id)) {
    id = name + "_" + std::to_string(suffix++);
  }
  MapLayer layer;
  layer.id = id;
  layer.name = name;
  layer.visible = true;
  active_layer_id_ = id;
  layers_.push_back(std::move(layer));
  return true;
}

bool LayerStore::remove_layer(const std::string& id) {
  const auto it =
      std::find_if(layers_.begin(), layers_.end(),
                   [&](const MapLayer& l) { return l.id == id; });
  if (it == layers_.end()) {
    return false;
  }
  layers_.erase(it);
  if (active_layer_id_ == id) {
    active_layer_id_ = layers_.empty() ? std::string() : layers_.front().id;
  }
  selected_id_ = {};
  return true;
}

bool LayerStore::set_layer_visible(const std::string& id, bool visible) {
  MapLayer* layer = find_layer(id);
  if (!layer) {
    return false;
  }
  layer->visible = visible;
  return true;
}

bool LayerStore::select_layer(const std::string& id) {
  if (!find_layer(id)) {
    return false;
  }
  active_layer_id_ = id;
  return true;
}

bool LayerStore::move_layer(const std::string& id, int delta) {
  if (delta == 0 || layers_.size() < 2) {
    return false;
  }
  int index = -1;
  for (size_t i = 0; i < layers_.size(); ++i) {
    if (layers_[i].id == id) {
      index = static_cast<int>(i);
      break;
    }
  }
  if (index < 0) {
    return false;
  }
  const int target = index + delta;
  if (target < 0 || target >= static_cast<int>(layers_.size())) {
    return false;
  }
  std::swap(layers_[static_cast<size_t>(index)],
            layers_[static_cast<size_t>(target)]);
  return true;
}

void LayerStore::add_sample_features(MapLayer* layer, const std::string& tag) {
  if (!layer) {
    return;
  }
  MapFeature road;
  road.id = next_feature_id();
  road.kind = GeomKind::kLine;
  road.points = {{80, 220}, {220, 140}, {420, 280}, {620, 200}};
  road.fields = {{"name", tag + " road"}, {"type", "line"}};
  layer->features.push_back(std::move(road));

  MapFeature parcel;
  parcel.id = next_feature_id();
  parcel.kind = GeomKind::kPolygon;
  parcel.points = {{160, 300}, {280, 300}, {280, 400}, {160, 400}, {160, 300}};
  parcel.fields = {{"name", tag + " parcel"}, {"type", "polygon"}};
  layer->features.push_back(std::move(parcel));

  MapFeature node;
  node.id = next_feature_id();
  node.kind = GeomKind::kPoint;
  node.points = {{320, 180}};
  node.fields = {{"name", tag + " node"}, {"type", "point"}};
  layer->features.push_back(std::move(node));
}

void LayerStore::clear_selection() {
  for (MapLayer& layer : layers_) {
    for (MapFeature& f : layer.features) {
      f.selected = false;
    }
  }
  selected_id_ = {};
}

bool LayerStore::select_feature(const content::FeatureId& id) {
  clear_selection();
  MapFeature* f = find_feature(id);
  if (!f) {
    return false;
  }
  f->selected = true;
  selected_id_ = id;
  return true;
}

const MapFeature* LayerStore::selected_feature() const {
  return find_feature(selected_id_);
}

void LayerStore::replace_layers(std::vector<MapLayer> loaded) {
  layers_.clear();
  layers_.swap(loaded);
  active_layer_id_ = layers_.empty() ? std::string() : layers_.front().id;
  selected_id_ = {};
}

}  // namespace detail
}  // namespace content
