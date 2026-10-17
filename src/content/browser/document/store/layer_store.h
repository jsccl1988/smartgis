// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_STORE_LAYER_STORE_H_
#define CONTENT_BROWSER_DOCUMENT_STORE_LAYER_STORE_H_

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "content/browser/document/store/map_layer.h"
#include "content/public/types.h"

namespace content {
namespace detail {

// Owns GisScene layer vector, active/selection ids, and CRUD helpers.
class LayerStore {
 public:
  void clear();

  // Serializes UI ingest/edit vs Display layout / inspector walks. Recursive so
  // feature_edit can hold the lock across create_layer + feature push.
  // Callers that return |layers()| references must hold this for that lifetime.
  std::recursive_mutex& mutex() const { return mu_; }

  // Monotonic layout identity for Map2d StaticReuse. Display threads must
  // read this instead of walking |layers_| (UI ingest races present).
  uint64_t content_revision() const {
    return content_revision_.load(std::memory_order_acquire);
  }
  // Caller must already hold mutex() when mutating outside LayerStore methods.
  void note_content_changed() {
    content_revision_.fetch_add(1, std::memory_order_acq_rel);
  }

  std::vector<MapLayer>& layers() { return layers_; }
  const std::vector<MapLayer>& layers() const { return layers_; }

  const std::string& active_layer_id() const { return active_layer_id_; }
  void set_active_layer_id(std::string id) {
    active_layer_id_ = std::move(id);
  }

  const content::FeatureId& selected_id() const { return selected_id_; }
  void set_selected_id(content::FeatureId id) { selected_id_ = id; }

  bool last_open_was_ogr() const { return last_open_was_ogr_; }
  void set_last_open_was_ogr(bool v) { last_open_was_ogr_ = v; }

  uint32_t next_id_value() const { return next_id_; }

  MapLayer* find_layer(const std::string& id);
  const MapLayer* find_layer(const std::string& id) const;
  MapFeature* find_feature(const content::FeatureId& id);
  const MapFeature* find_feature(const content::FeatureId& id) const;

  content::FeatureId next_feature_id();
  void ensure_active_layer_or_front();

  std::vector<content::LayerDesc> layer_descs() const;
  size_t feature_count() const;

  // |kind| defaults to kVector (digitize / OGR-style vector layer).
  bool create_layer(const std::string& name,
                    content::LayerKind kind = content::LayerKind::kVector);
  bool remove_layer(const std::string& id);
  bool set_layer_visible(const std::string& id, bool visible);
  bool select_layer(const std::string& id);
  bool move_layer(const std::string& id, int delta);

  void clear_selection();
  bool select_feature(const content::FeatureId& id);
  const MapFeature* selected_feature() const;

  // Replace all layers after a successful OGR ingest (clears selection).
  void replace_layers(std::vector<MapLayer> loaded);

 private:
  mutable std::recursive_mutex mu_;
  std::vector<MapLayer> layers_;
  std::string active_layer_id_;
  content::FeatureId selected_id_{};
  uint32_t next_id_ = 1;
  bool last_open_was_ogr_ = false;
  // Starts at 1 so a never-mutated empty store still differs from zeroed fp.
  std::atomic<uint64_t> content_revision_{1};
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_STORE_LAYER_STORE_H_
