// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_GIS_DOCUMENT_H_
#define CONTENT_PUBLIC_GIS_DOCUMENT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "content/public/types.h"
#include "tool/draft/draft.h"

namespace content {

// Narrow GIS document edit API (layers / features / style JSON / mesh /
// extent). Owned by GisContents (capability host); PluginHost only forwards.
// Concrete adapters live under content/browser/document/.
class GisDocument {
 public:
  virtual ~GisDocument() = default;

  virtual bool create_layer(std::string_view name,
                            std::string_view geometry_type) {
    (void)name;
    (void)geometry_type;
    return false;
  }
  virtual bool remove_layer(std::string_view id) {
    (void)id;
    return false;
  }
  virtual bool set_layer_visible(std::string_view id, bool visible) {
    (void)id;
    (void)visible;
    return false;
  }
  virtual size_t layer_count() const { return 0; }
  virtual size_t feature_count() const { return 0; }

  virtual FeatureId append_from_draft(
      const tool::Draft& draft, const char* tool_id,
      const std::function<void(int view_x, int view_y, double* map_x,
                               double* map_y)>& to_map) {
    (void)draft;
    (void)tool_id;
    (void)to_map;
    return FeatureId{};
  }

  virtual bool update_feature_field(std::string_view token,
                                    std::string_view field,
                                    std::string_view value) {
    (void)token;
    (void)field;
    (void)value;
    return false;
  }

  virtual bool apply_style_json(std::string_view json) {
    (void)json;
    return false;
  }

  virtual bool add_triangle_mesh(std::string_view name, const double* xyz,
                                 int point_count, const int* triangles,
                                 int triangle_count) {
    (void)name;
    (void)xyz;
    (void)point_count;
    (void)triangles;
    (void)triangle_count;
    return false;
  }

  // Point cloud layer (xyz interleaved float; rgba optional RGBA8).
  virtual bool add_point_cloud(std::string_view name, const float* xyz,
                               int point_count, const uint8_t* rgba) {
    (void)name;
    (void)xyz;
    (void)point_count;
    (void)rgba;
    return false;
  }

  virtual bool compute_extent(Extent2* out) const {
    (void)out;
    return false;
  }

  virtual void notify_layers_changed() {}

  // Selection / catalog / legend — formerly on GisContents. Default no-ops;
  // GisSceneDocument publishes EventBus; GisContents pipe wrapper also
  // forwards to the OOP host pipe when present.
  virtual void set_selection(uint32_t view_id,
                             const FeatureId* ids,
                             size_t n) {
    (void)view_id;
    (void)ids;
    (void)n;
  }
  virtual void catalog_call(const char* json_op) { (void)json_op; }
  virtual void legend_snapshot(uint32_t view_id) { (void)view_id; }

  // After a successful style mutation; publishes StyleChanged when events
  // are bound.
  virtual void notify_style_changed(uint32_t view_id = 0) { (void)view_id; }

  static std::string feature_token(const FeatureId& id) {
    return encode_feature_token(id);
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_GIS_DOCUMENT_H_
