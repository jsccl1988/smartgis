// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/gis_document.h"

#include "content/browser/document/gis_scene.h"
#include "content/public/event_bus.h"
#include "gis/style/document/style_document.h"

#include <memory>
#include <string>

namespace content {

GisSceneDocument::GisSceneDocument(GisScene* scene, EventBus* events)
    : scene_(scene), events_(events) {}

bool GisSceneDocument::create_layer(std::string_view name,
                                       std::string_view geometry_type) {
  if (!scene_ || name.empty()) {
    return false;
  }
  if (!scene_->create_layer(std::string(name), std::string(geometry_type))) {
    return false;
  }
  notify_layers_changed();
  return true;
}

bool GisSceneDocument::remove_layer(std::string_view id) {
  if (!scene_ || id.empty()) {
    return false;
  }
  if (!scene_->remove_layer(std::string(id))) {
    return false;
  }
  notify_layers_changed();
  return true;
}

bool GisSceneDocument::set_layer_visible(std::string_view id, bool visible) {
  if (!scene_ || id.empty()) {
    return false;
  }
  if (!scene_->set_layer_visible(std::string(id), visible)) {
    return false;
  }
  notify_layers_changed();
  return true;
}

size_t GisSceneDocument::layer_count() const {
  return scene_ ? scene_->layer_count() : 0;
}

size_t GisSceneDocument::feature_count() const {
  return scene_ ? scene_->feature_count() : 0;
}

FeatureId GisSceneDocument::append_from_draft(
    const tool::Draft& draft, const char* tool_id,
    const std::function<void(int view_x, int view_y, double* map_x,
                             double* map_y)>& to_map) {
  if (!scene_) {
    return FeatureId{};
  }
  return scene_->append_from_draft(draft, tool_id, to_map);
}

bool GisSceneDocument::update_feature_field(std::string_view token,
                                               std::string_view field,
                                               std::string_view value) {
  if (!scene_ || token.empty() || field.empty()) {
    return false;
  }
  return scene_->update_feature_field(std::string(token), std::string(field),
                                      std::string(value));
}

bool GisSceneDocument::apply_style_json(std::string_view json) {
  if (!scene_ || json.empty()) {
    return false;
  }
  auto style = std::make_shared<gis::style::StyleDocument>();
  if (!gis::style::parse_style_document(std::string(json), style.get())) {
    scene_->clear_style_document();
    return false;
  }
  scene_->set_style_document(std::move(style));
  return true;
}

bool GisSceneDocument::add_triangle_mesh(std::string_view name,
                                            const double* xyz, int point_count,
                                            const int* triangles,
                                            int triangle_count) {
  if (!scene_ || name.empty() || !xyz || point_count <= 0 || !triangles ||
      triangle_count <= 0) {
    return false;
  }
  if (!scene_->add_triangle_layer(std::string(name), xyz, point_count,
                                  triangles, triangle_count)) {
    return false;
  }
  notify_layers_changed();
  return true;
}

bool GisSceneDocument::add_point_cloud(std::string_view name,
                                          const float* xyz, int point_count,
                                          const uint8_t* rgba) {
  if (!scene_ || name.empty() || !xyz || point_count <= 0) {
    return false;
  }
  if (!scene_->add_point_cloud_layer(std::string(name), xyz, point_count,
                                     rgba)) {
    return false;
  }
  notify_layers_changed();
  return true;
}

bool GisSceneDocument::compute_extent(Extent2* out) const {
  if (!scene_ || !out) {
    return false;
  }
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  if (!scene_->compute_extent(&min_x, &min_y, &max_x, &max_y)) {
    return false;
  }
  out->xmin = min_x;
  out->ymin = min_y;
  out->xmax = max_x;
  out->ymax = max_y;
  return true;
}

void GisSceneDocument::notify_layers_changed() {
  if (!events_) {
    return;
  }
  LayersChanged e;
  e.layer_count = static_cast<uint32_t>(layer_count());
  events_->publish(e);
}

}  // namespace content
