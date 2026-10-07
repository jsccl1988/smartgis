// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU prep before tess: envelope reject, sub-pixel cull, overview holes.
// IR stays view-CRS; no GDI POINT / MapPainter / HDC.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_PACK_H_
#define VISTA_COMPONENT_MAP_LAYOUT_PACK_H_

#include <memory>
#include <vector>

#include "vista/component/map/batch.h"
#include "vista/component/map/layout.h"

class OGRGeometry;

namespace vista {
namespace detail {

// Filtered LayerBatch views. owned holds hole-stripped clones (overview).
struct PackedGeoms {
  std::vector<LayerBatch> batches;
  std::vector<std::unique_ptr<OGRGeometry>> owned;

  PackedGeoms();
  ~PackedGeoms();
  PackedGeoms(PackedGeoms&&) noexcept;
  PackedGeoms& operator=(PackedGeoms&&) noexcept;
  PackedGeoms(const PackedGeoms&) = delete;
  PackedGeoms& operator=(const PackedGeoms&) = delete;
};

PackedGeoms pack_geoms(const LayoutInput& in,
                       const std::vector<LayerBatch>& layers);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_PACK_H_
