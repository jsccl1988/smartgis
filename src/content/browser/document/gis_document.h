// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_GIS_DOCUMENT_H_
#define CONTENT_BROWSER_DOCUMENT_GIS_DOCUMENT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

#include "content/public/gis_document.h"

namespace content {

class EventBus;
class GisScene;

// GisScene-backed GisDocument adapter. Not part of the embedder public API;
// shell constructs this and GisContents::take_gis_document owns it.
class GisSceneDocument final : public GisDocument {
 public:
  explicit GisSceneDocument(GisScene* scene, EventBus* events = nullptr);

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
  GisScene* scene_ = nullptr;
  EventBus* events_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_GIS_DOCUMENT_H_
