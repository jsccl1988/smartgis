// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_EDIT_FEATURE_EDIT_H_
#define CONTENT_BROWSER_DOCUMENT_EDIT_FEATURE_EDIT_H_

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

const MapFeature* hit_test(LayerStore* store, double map_x, double map_y,
                           double tol_map);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_EDIT_FEATURE_EDIT_H_
