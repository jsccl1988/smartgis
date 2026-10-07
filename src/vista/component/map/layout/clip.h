// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Tile-skirt intersection used by painters. Envelope reject and hole
// stripping stay in stage/pack; this header does not know about stages.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_CLIP_H_
#define VISTA_COMPONENT_MAP_LAYOUT_CLIP_H_

#include <memory>
#include <vector>

#include "vista/component/map/layout/slice_key.h"

class OGRGeometry;

namespace vista {
namespace detail {

// Intersection with tile skirt AABB. nullptr *use means skip.
// When the envelope is already inside the skirt, *use stays the source geom.
bool prepare_tile_clip(const OGRGeometry* geom, const LayoutTile* tile,
                       std::vector<std::unique_ptr<OGRGeometry>>* store,
                       const OGRGeometry** use);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_CLIP_H_
