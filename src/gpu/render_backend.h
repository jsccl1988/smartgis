// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_RENDER_BACKEND_H_
#define GPU_RENDER_BACKEND_H_

#include "content/public/map_types.h"
#include "gpu/present.h"

#include <cstdint>
#include <functional>
#include <string>

// GPU-process map paint switch. Chrome must not include this header.
// Track B = existing demo / RHI path. Track A = MapLibre-style 2D basemap.
namespace gpu {

enum class RenderBackendKind {
  kTrackBRhi = 0,
  kTrackAMapLibre = 1,
};

struct TileFetchResult {
  bool ok = false;
  std::string body;
};

using TileFetchFn = std::function<TileFetchResult(const std::string& url)>;

// SMT_MAP_BACKEND=a|track_a|maplibre selects Track A. Default Track B.
RenderBackendKind select_render_backend();
const char* render_backend_name(RenderBackendKind kind);

// True only when this binary was compiled with smt_enable_maplibre=true.
bool maplibre_runtime_compiled();
// True only when a real mln::Map / mbgl::Map was linked from the pin.
bool maplibre_map_linked();

struct MapPaintRequest {
  content::ViewKind kind = content::ViewKind::kMapEdit;
  content::Extent2 extent{};
  const char* style_json = nullptr;
  const char* tile_url_template = nullptr;
  TileFetchFn fetch;
};

// Paints a 2D basemap into |present| (DIB and/or DXGI). Never touches chrome
// HWNDs. Scene3d callers should keep Track B / GpuScene.
// |fetch| is the TileProvider consumer hook (inject or wrap TileProvider).
bool paint_map_frame(detail::PresentTarget* present, const MapPaintRequest& req);

}  // namespace gpu

#endif  // GPU_RENDER_BACKEND_H_
