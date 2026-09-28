// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/display/display.h"

#include "gpu/compositor/composer/composer.h"
#include "gpu/device/gpu_device_hub.h"
#include "gpu/raster/direct/direct.h"
#include "gpu/raster/tile/tile_quads.h"

#include <cstring>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gpu {
namespace {

bool env_selects_tile() {
  char buf[32] = {};
  const DWORD n = GetEnvironmentVariableA("SMT_MAP_BACKEND", buf, sizeof(buf));
  if (n == 0 || n >= sizeof(buf)) {
    return false;
  }
  return std::strcmp(buf, "a") == 0 || std::strcmp(buf, "A") == 0 ||
         std::strcmp(buf, "track_a") == 0 ||
         std::strcmp(buf, "maplibre") == 0;
}

bool g_has_override = false;
ContentSource g_override = ContentSource::kDirect;

detail::AdapterId ensure_surface_adapter(detail::OutputSurface* surface) {
  detail::GpuDeviceHub& hub = detail::device_hub();
  if (!surface) {
    return hub.primary_adapter();
  }
  detail::AdapterId id = hub.adapter_of(surface);
  if (id == detail::kAdapterInvalid) {
    id = surface->adapter_id();
  }
  if (id == detail::kAdapterInvalid) {
    id = hub.primary_adapter();
  }
  (void)hub.bind_surface(surface, id);
  return id;
}

// Stable shell backing for generation reuse (pointer valid across frames).
struct ShellRasterCache {
  uint64_t generation = 0;
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  std::vector<uint8_t> packed;
};

ShellRasterCache& shell_raster_cache() {
  static ShellRasterCache cache;
  return cache;
}

void attach_shell_raster(detail::RenderPass* pass,
                         const ui::gfx::ShellRaster& shell,
                         uint64_t shell_generation) {
  if (!pass || pass->width_px == 0 || pass->height_px == 0) {
    return;
  }
  ShellRasterCache& cache = shell_raster_cache();
  // Unchanged generation: skip shell→packed memcpy. RHI reuses texture via
  // texture_cache_key; software path reads the stable cache pointer.
  if (shell_generation != 0 && shell_generation == cache.generation &&
      cache.width_px == pass->width_px && cache.height_px == pass->height_px &&
      !cache.packed.empty()) {
    detail::append_bgra_quad_external(pass, cache.packed.data(),
                                      pass->width_px * 4u, 1.f, false,
                                      shell_generation);
    return;
  }
  if (!shell.bgra || shell.width_px == 0 || shell.height_px == 0) {
    return;
  }
  if (shell.width_px != pass->width_px || shell.height_px != pass->height_px) {
    return;
  }
  const uint32_t stride =
      shell.stride_bytes != 0 ? shell.stride_bytes : shell.width_px * 4u;
  if (stride < shell.width_px * 4u) {
    return;
  }
  std::vector<uint8_t> packed(
      static_cast<size_t>(pass->width_px) * pass->height_px * 4u);
  for (uint32_t y = 0; y < pass->height_px; ++y) {
    std::memcpy(packed.data() + static_cast<size_t>(y) * pass->width_px * 4u,
                shell.bgra + static_cast<size_t>(y) * stride,
                static_cast<size_t>(pass->width_px) * 4u);
  }
  const uint64_t cache_key = shell_generation;
  if (shell_generation != 0) {
    cache.generation = shell_generation;
    cache.width_px = pass->width_px;
    cache.height_px = pass->height_px;
    cache.packed = packed;
  }
  detail::append_bgra_quad(pass, std::move(packed), 1.f, false, cache_key);
}

bool present_frame(detail::OutputSurface* surface,
                   const detail::CompositorFrame& frame) {
  const detail::AdapterId adapter = ensure_surface_adapter(surface);
  auto composer = detail::make_frame_composer(detail::select_compose_backend(),
                                              adapter);
  return composer->draw_frame(surface, frame);
}

void submit_direct(detail::OutputSurface* surface, const DrawRequest& req) {
  if (!surface) {
    return;
  }
  const uint32_t w = surface->wire().width_px;
  const uint32_t h = surface->wire().height_px;
  if (w == 0 || h == 0) {
    return;
  }
  detail::CompositorFrame frame;
  frame.width_px = w;
  frame.height_px = h;
  detail::RenderPass pass;
  detail::raster_direct_quads(&pass, req.kind, w, h);
  attach_shell_raster(&pass, req.shell, req.shell_generation);
  frame.render_pass_list.push_back(std::move(pass));
  (void)present_frame(surface, frame);
}

}  // namespace

ContentSource select_content_source() {
  if (g_has_override) {
    return g_override;
  }
  if (env_selects_tile()) {
    return ContentSource::kTile;
  }
  return ContentSource::kDirect;
}

void set_content_source(ContentSource source) {
  g_has_override = true;
  g_override = source;
}

void clear_content_source_override() {
  g_has_override = false;
}

bool apply_content_source_command(const char* command_id) {
  if (!command_id || command_id[0] == '\0') {
    return false;
  }
  if (std::strcmp(command_id, kCmdContentTile) == 0) {
    set_content_source(ContentSource::kTile);
    return true;
  }
  if (std::strcmp(command_id, kCmdContentDirect) == 0) {
    set_content_source(ContentSource::kDirect);
    return true;
  }
  return false;
}

const char* content_source_name(ContentSource source) {
  return source == ContentSource::kTile ? "tile" : "direct";
}

bool draw_and_swap(detail::OutputSurface* surface, const DrawRequest& req) {
  // Scene3d is direct content, not a failure and not a third mode.
  if (req.kind == content::ViewKind::kScene3d) {
    if (!surface) {
      return false;
    }
    submit_direct(surface, req);
    return true;
  }
  if (select_content_source() == ContentSource::kTile) {
    detail::RenderPass pass;
    if (detail::raster_tile_quads(surface, req, &pass)) {
      attach_shell_raster(&pass, req.shell, req.shell_generation);
      detail::CompositorFrame frame;
      frame.width_px = pass.width_px;
      frame.height_px = pass.height_px;
      frame.render_pass_list.push_back(std::move(pass));
      return present_frame(surface, frame);
    }
    // Null surface or zero size. A null surface cannot be cleared.
    if (!surface) {
      return false;
    }
    if (surface->wire().width_px == 0 || surface->wire().height_px == 0) {
      submit_direct(surface, req);
      return true;
    }
    return false;
  }
  if (!surface) {
    return false;
  }
  submit_direct(surface, req);
  return true;
}

}  // namespace gpu
