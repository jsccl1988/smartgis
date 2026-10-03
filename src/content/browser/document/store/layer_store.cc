// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/store/layer_store.h"

#include <algorithm>
#include <cstring>

namespace content {
namespace detail {
namespace {

// True for china_city PLPT stems produced by split_layers_by_kind_field.
bool is_china_plpt_id(const std::string& id) {
  return id == "china.area" || id == "china.line" || id == "china.point" ||
         id == "china.text";
}

content::LayerKind resolve_layer_kind(const MapLayer& layer) {
  if (layer.kind != content::LayerKind::kUnknown) {
    return layer.kind;
  }
  // MapLayer only stores vector MapFeature geometry today.
  if (!layer.features.empty()) {
    return content::LayerKind::kVector;
  }
  return content::LayerKind::kUnknown;
}

content::LayerDesc make_leaf_desc(const MapLayer& layer,
                                  const std::string& active_id) {
  content::LayerDesc d;
  d.id = layer.id;
  d.name = layer.name;
  d.visible = layer.visible;
  d.active = (layer.id == active_id);
  d.kind = resolve_layer_kind(layer);
  d.expanded = true;
  return d;
}

}  // namespace

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
  // Cap reserve: a skewed MapSession/MapScene layout (parallel out/Debug
  // rebuild) can leave layers_ as MSVC debug-fill; size() then looks like
  // ~10^18 and vector::reserve throws std::length_error / process abort.
  constexpr size_t kMaxLayers = 1u << 20;
  const size_t n = layers_.size();
  if (n > kMaxLayers) {
    return out;
  }

  std::vector<content::LayerDesc> china;
  std::vector<content::LayerDesc> other;
  china.reserve(4);
  other.reserve(n);
  bool china_any_visible = false;
  for (const MapLayer& layer : layers_) {
    content::LayerDesc d = make_leaf_desc(layer, active_layer_id_);
    if (is_china_plpt_id(layer.id)) {
      china_any_visible = china_any_visible || d.visible;
      china.push_back(std::move(d));
    } else {
      other.push_back(std::move(d));
    }
  }

  // Nest china_city PLPT under one group when the store has that split set.
  // Fewer than two leaves stay flat (no invented singleton group).
  if (china.size() >= 2) {
    content::LayerDesc group;
    group.id = "group.china";
    group.name = "China";
    group.visible = china_any_visible;
    group.active = false;
    group.kind = content::LayerKind::kGroup;
    group.expanded = true;
    group.children = std::move(china);
    out.reserve(1 + other.size());
    out.push_back(std::move(group));
    for (content::LayerDesc& d : other) {
      out.push_back(std::move(d));
    }
    return out;
  }

  out.reserve(china.size() + other.size());
  for (content::LayerDesc& d : china) {
    out.push_back(std::move(d));
  }
  for (content::LayerDesc& d : other) {
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

bool LayerStore::create_layer(const std::string& name,
                              content::LayerKind kind) {
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
  layer.kind = kind;
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
  if (layer->kind == content::LayerKind::kUnknown) {
    layer->kind = content::LayerKind::kVector;
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
