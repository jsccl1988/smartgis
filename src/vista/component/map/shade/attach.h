// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Attaches a DEM hillshade TileSlot onto LayoutInput. The host supplies
// skip / ready policy; path lookup and bake_hillshade_slot stay here.

#ifndef VISTA_COMPONENT_MAP_SHADE_ATTACH_H_
#define VISTA_COMPONENT_MAP_SHADE_ATTACH_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vista/component/map/layout.h"
#include "vista/vista_export.h"

namespace vista {

// Host flags for one layout. GisScene extent, land visibility, and process
// switches stay with the caller. texture_key is the host id on the slot.
struct HillshadeAttachPolicy {
  bool skip = false;
  bool ready = false;
  TileSlot ready_slot{};
  uint32_t texture_key = 0;
};

// Why attach_hillshade_slot did or did not write LayoutInput::hillshade_tiles.
enum class HillshadeAttachKind : uint8_t {
  kSkipped = 0,
  kReused,
  kDemMissing,
  kBaked,
  kBakeFailed,
  kNoLayer,
};

// Pixels are filled only for kBaked. elapsed_ms covers the bake call for
// kBaked and kBakeFailed (path lookup is outside the clock).
struct HillshadeAttachResult {
  HillshadeAttachKind kind = HillshadeAttachKind::kSkipped;
  std::string dem_path;
  std::vector<uint8_t> rgba;
  int width = 0;
  int height = 0;
  bool bake_ok = false;
  size_t rgba_bytes = 0;
  TileSlot slot{};
  int64_t elapsed_ms = 0;
  size_t tile_count = 0;
  float opacity = 0.f;
  uint32_t texture_key = 0;
  double zoom = 0;
};

// Writes have_dem_clip / dem_clip / hillshade_tiles when a slot is reused or
// freshly baked. A null layout returns kSkipped.
VISTA_EXPORT HillshadeAttachResult attach_hillshade_slot(
    LayoutInput* layout, const HillshadeAttachPolicy& policy);

}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_SHADE_ATTACH_H_
