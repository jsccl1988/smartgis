// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_RENDER_BACKEND_H_
#define GPU_RENDER_BACKEND_H_

#include "content/public/map_types.h"
#include "gpu/present.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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

// Product HTTP(S) fetch for MapPaintRequest::fetch. Wraps net::HttpClient::get
// (same stack TileProvider uses when no inject). Never throws; ok=false on
// transport / non-2xx / empty body so paint can keep background.
TileFetchFn make_net_tile_fetch();

// SMT_MAP_BACKEND=a|track_a|maplibre selects Track A. Default Track B.
RenderBackendKind select_render_backend();
const char* render_backend_name(RenderBackendKind kind);

// True only when this binary was compiled with smt_enable_maplibre=true.
bool maplibre_runtime_compiled();
// True only when a real mln::Map / mbgl::Map was linked from the pin.
bool maplibre_map_linked();

// Request for Track A still-image paint into PresentTarget.
struct MapPaintRequest {
  content::ViewKind kind = content::ViewKind::kMapEdit;
  // Web Mercator viewport (EPSG:3857). Degenerate → single tile z/x/y=0/0/0.
  content::Extent2 extent{};
  // XYZ zoom. < 0 → sdb::tile::estimate_zoom(extent) when extent is valid.
  int zoom = -1;
  const char* style_json = nullptr;
  // Single XYZ template (legacy). Used when |tile_url_templates| is empty.
  const char* tile_url_template = nullptr;
  // Optional multi-template list; assigned to raster layers without a Style
  // source id, in document order. Style `sources` + layer.source take priority.
  std::vector<std::string> tile_url_templates;
  TileFetchFn fetch;
};

// Paints a 2D basemap into |present| (DIB and/or DXGI). Never touches chrome
// HWNDs. Scene3d callers should keep Track B / GpuScene.
// |fetch| is the TileProvider consumer hook (inject or wrap TileProvider).
bool paint_map_frame(detail::PresentTarget* present, const MapPaintRequest& req);

}  // namespace gpu

#endif  // GPU_RENDER_BACKEND_H_
