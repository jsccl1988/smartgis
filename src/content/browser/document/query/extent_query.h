// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_QUERY_EXTENT_QUERY_H_
#define CONTENT_BROWSER_DOCUMENT_QUERY_EXTENT_QUERY_H_

#include <vector>

#include "content/browser/document/store/layer_store.h"
#include "content/public/types.h"
#include "vista/terrain/dem/mask/land_mask.h"

namespace content {
namespace detail {

bool compute_extent(const LayerStore& store, double* min_x, double* min_y,
                    double* max_x, double* max_y);

bool has_china_extent(const LayerStore& store);

bool active_layer_world_extent(const LayerStore& store, content::Extent2* out);

bool selection_world_extent(const LayerStore& store, content::Extent2* out);

content::Extent2 world_extent(const LayerStore& store);

void export_land_rings(const LayerStore& store,
                       std::vector<vista::LonLatRing>* out);

bool polygon_fit_box(const LayerStore& store, double* min_x, double* min_y,
                     double* max_x, double* max_y);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_QUERY_EXTENT_QUERY_H_
