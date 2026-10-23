// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_HORIZON_DETAIL_CATALOG_LAYER_FILTER_H_
#define APP_VIEWS_UI_HORIZON_DETAIL_CATALOG_LAYER_FILTER_H_

#include <string>
#include <vector>

#include "content/public/types.h"

namespace app {
namespace detail {

// Catalog / Map view pairing: 2D catalog ↔ Map tab, 3D catalog ↔ Scene3d tab.
enum class CatalogViewMode {
  kMap2d = 0,
  kScene3d = 1,
};

// True when a leaf (or named node) belongs in |mode|'s TOC.
// Same GisDocument: Map2d shows carto leaves; Scene3d shows land-surface
// affinity (Both). No content/public LayerDesc change — affinity is inferred.
inline bool catalog_leaf_matches_mode(const std::string& id,
                                      const std::string& name,
                                      CatalogViewMode mode) {
  const bool land = name == "area" || name == "Land" || name == "land" ||
                    id == "layer.area" || id.find("land") != std::string::npos;
  const bool map_only = id == "style.hillshade" || id == "basemap.tiles" ||
                        name == "hillshade" || name == "Basemap";
  switch (mode) {
    case CatalogViewMode::kScene3d:
      // Scene TOC: land surface (+ future scene-only ids). Map-only rasters out.
      return land && !map_only;
    case CatalogViewMode::kMap2d:
    default:
      // Map TOC: everything except pure scene-only (none yet → all leaves).
      return true;
  }
}

// Keep nodes that match |mode|, or groups that retain at least one child.
inline std::vector<content::LayerDesc> filter_layer_descs_for_mode(
    const std::vector<content::LayerDesc>& in,
    CatalogViewMode mode) {
  std::vector<content::LayerDesc> out;
  out.reserve(in.size());
  for (const content::LayerDesc& node : in) {
    content::LayerDesc copy = node;
    if (!node.children.empty()) {
      copy.children = filter_layer_descs_for_mode(node.children, mode);
      if (copy.children.empty() &&
          !catalog_leaf_matches_mode(node.id, node.name, mode)) {
        continue;
      }
      out.push_back(std::move(copy));
      continue;
    }
    if (catalog_leaf_matches_mode(node.id, node.name, mode)) {
      out.push_back(std::move(copy));
    }
  }
  return out;
}

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_UI_HORIZON_DETAIL_CATALOG_LAYER_FILTER_H_
