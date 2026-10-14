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

// True when any visible feature vertices yield a non-empty envelope.
bool has_drawable_extent(const LayerStore& store);

// True when at least one layer carries LayerRole::kLandSurface.
bool has_land_surface(const LayerStore& store);

// True when a land-surface layer is visible. False when none exist or all
// land-surface layers are hidden.
bool land_surface_visible(const LayerStore& store);

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
