// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_FRAME_SINK_H_
#define GPU_FRAME_SINK_H_

#include "content/public/map_layer_types.h"
#include "gpu/display/output_surface.h"
#include "ui/gfx/raster/shell_raster.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Public draw boundary for the GPU process and its tests.
// Direct content is the demo grid and the Scene3d DEM underlay.
// Tile content is a StyleDocument plus XYZ tiles.
// Shell must not include this header.
namespace gpu {

// Wire integer 0 is direct; 1 is tile.
enum class ContentSource {
  kDirect = 0,
  kTile = 1,
};

struct TileFetchResult {
  bool ok = false;
  std::string body;
};

using TileFetchFn = std::function<TileFetchResult(const std::string& url)>;

// Product HTTP(S) fetch for DrawRequest::fetch. Wraps net::HttpClient::get
// (same stack TileProvider uses when no inject). Never throws; ok=false on
// transport / non-2xx / empty body so the draw can keep background.
TileFetchFn make_net_tile_fetch();

// Request for one map frame into OutputSurface.
struct DrawRequest {
  content::ViewKind kind = content::ViewKind::kMapEdit;
  // Web Mercator viewport (EPSG:3857). Degenerate → single tile z/x/y=0/0/0.
  content::Extent2 extent{};
  // XYZ zoom. < 0 → gis::tile::estimate_zoom(extent) when extent is valid.
  int zoom = -1;
  const char* style_json = nullptr;
  // Single XYZ template (legacy). Used when |tile_url_templates| is empty.
  const char* tile_url_template = nullptr;
  // Optional multi-template list; assigned to raster layers without a Style
  // source id, in document order. Style `sources` + layer.source take priority.
  std::vector<std::string> tile_url_templates;
  TileFetchFn fetch;
  // Application shell raster from ui::gfx (Views menus and panels over the
  // map pane). BGRA8, top-down. Empty bgra skips the quad.
  // Size must match the surface (pane-cropped Commit). Src-over after map
  // quads: alpha 0 keeps the map visible; an opaque buffer covers it.
  // ShellRaster does not blend this. Pointer is only valid for the Submit
  // that copies it — PresentMailbox / attach_shell_raster copy (or reuse by
  // shell_generation) before return. FlyCube in-process present fills the
  // same fields via MapViewport::snapshot_shell_overlay → present_gpu.
  ui::gfx::ShellRaster shell;
  // Non-zero: skip full shell memcpy when unchanged; also sets DrawQuad
  // texture_cache_key so RHI can reuse the uploaded texture.
  uint64_t shell_generation = 0;
};

// Draws one frame into |surface| and presents once. Returns true when the
// call matches the previous success contract. For async present from UI /
// shell threads, enqueue via PresentMailbox::submit (display/present_mailbox.h)
// instead of calling this on the UI thread.
// 2D kinds follow select_content_source. set_content_source beats the
// environment. MAP_BACKEND=a|track_a|maplibre selects tile; every other
// value selects direct. Scene3d is direct content inside this call: the demo
// frame is drawn and the function returns true. It is not a failure and not
// a third mode. If tile fails because the surface is null or its size is 0,
// a non-null surface takes the direct clear so those pixels match the
// previous failure path.
bool draw_and_swap(detail::OutputSurface* surface, const DrawRequest& req);

// Command ids stay as wire strings. rhi selects direct; maplibre selects tile.
inline constexpr const char kCmdContentDirect[] = "view.backend.rhi";
inline constexpr const char kCmdContentTile[] = "view.backend.maplibre";

ContentSource select_content_source();
void set_content_source(ContentSource source);
void clear_content_source_override();
// True when |command_id| is view.backend.rhi or view.backend.maplibre.
bool apply_content_source_command(const char* command_id);
// "direct" or "tile".
const char* content_source_name(ContentSource source);

}  // namespace gpu

#endif  // GPU_FRAME_SINK_H_
