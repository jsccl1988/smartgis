// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_EDIT_FEATURE_EDIT_H_
#define CONTENT_BROWSER_DOCUMENT_EDIT_FEATURE_EDIT_H_

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "content/browser/document/store/layer_store.h"
#include "content/public/map_types.h"
#include "tool/draft/draft.h"

namespace content {
namespace detail {

content::FeatureId append_from_draft(
    LayerStore* store, const tool::Draft& draft, const char* tool_id,
    const std::function<void(int view_x, int view_y, double* map_x,
                             double* map_y)>& to_map);

content::FeatureId move_selected_vertex(LayerStore* store, double map_x,
                                        double map_y, double tol_map);

bool copy_feature_xy(const LayerStore& store, const content::FeatureId& id,
                     std::vector<std::pair<double, double>>* out);

bool add_triangle_layer(LayerStore* store, const std::string& name,
                        const double* xyz, int point_count,
                        const int* triangles, int triangle_count);

// Point features from interleaved XYZ (map XY = X,Y; Z stored in fields).
// Optional |rgba| (4 * point_count) writes per-point #RRGGBB into field
// "color" for data-driven circle paint.
bool add_point_cloud_layer(LayerStore* store, const std::string& name,
                           const float* xyz, int point_count,
                           const uint8_t* rgba = nullptr);

const MapFeature* hit_test(LayerStore* store, double map_x, double map_y,
                           double tol_map);

// Snap result in map CRS. kind distinguishes vertex vs edge projection.
struct SnapHit {
  enum class Kind { kNone = 0, kVertex = 1, kEdge = 2 };
  Kind kind = Kind::kNone;
  double x = 0;
  double y = 0;
  content::FeatureId feature_id{};
  int vertex_index = -1;  // valid for kVertex; start vertex of edge for kEdge
};

// Vertex then edge snap against all visible features. |tol_map| is the search
// radius in map CRS units. Returns kNone when nothing is within tolerance.
SnapHit snap_to_features(const LayerStore& store, double map_x, double map_y,
                         double tol_map);

// Convenience: write snapped coordinates into |out_x|/|out_y| when hit.
bool snap_point(const LayerStore& store, double map_x, double map_y,
                double tol_map, double* out_x, double* out_y);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_EDIT_FEATURE_EDIT_H_
