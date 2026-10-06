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

#include "content/public/map_layer_types.h"
#include "tool/draft/draft.h"

namespace content {

class EventBus;
class MapScene;

// Narrow GIS document plugins may mutate (layers / features / style JSON /
// triangle mesh / extent). Wraps MapScene in the shell; not MapContents.
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

  static std::string feature_token(const FeatureId& id) {
    return encode_feature_token(id);
  }
};

// Shell / tests: wrap a MapScene. Product TUs use PluginHost::gis_document().
class MapSceneGisDocument final : public GisDocument {
 public:
  explicit MapSceneGisDocument(MapScene* scene, EventBus* events = nullptr);

  bool create_layer(std::string_view name,
                    std::string_view geometry_type) override;
  bool remove_layer(std::string_view id) override;
  bool set_layer_visible(std::string_view id, bool visible) override;
  size_t layer_count() const override;
  size_t feature_count() const override;
  FeatureId append_from_draft(
      const tool::Draft& draft, const char* tool_id,
      const std::function<void(int view_x, int view_y, double* map_x,
                               double* map_y)>& to_map) override;
  bool update_feature_field(std::string_view token, std::string_view field,
                            std::string_view value) override;
  bool apply_style_json(std::string_view json) override;
  bool add_triangle_mesh(std::string_view name, const double* xyz,
                         int point_count, const int* triangles,
                         int triangle_count) override;
  bool add_point_cloud(std::string_view name, const float* xyz,
                       int point_count, const uint8_t* rgba) override;
  bool compute_extent(Extent2* out) const override;
  void notify_layers_changed() override;

 private:
  MapScene* scene_ = nullptr;
  EventBus* events_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_GIS_DOCUMENT_H_
