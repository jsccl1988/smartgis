// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/render_backend.h"

#include "gpu/maplibre_adapter.h"
#include "gpu/maplibre_runtime.h"

#include "net/http/http.h"

#include <cstring>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gpu {
namespace {

bool env_is_track_a() {
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
RenderBackendKind g_override = RenderBackendKind::kTrackBRhi;

}  // namespace

TileFetchFn make_net_tile_fetch() {
  return [](const std::string& url) -> TileFetchResult {
    TileFetchResult out;
    if (url.empty()) {
      return out;
    }
    try {
      net::HttpClient client;
      const net::HttpResult hr = client.get(url, /*timeout_sec=*/5);
      if (!hr.ok || hr.body.empty()) {
        return out;
      }
      out.ok = true;
      out.body = hr.body;
    } catch (...) {
      // Paint path must not throw; caller treats ok=false as skip-raster.
    }
    return out;
  };
}

RenderBackendKind select_render_backend() {
  if (g_has_override) {
    return g_override;
  }
  if (env_is_track_a()) {
    return RenderBackendKind::kTrackAMapLibre;
  }
  return RenderBackendKind::kTrackBRhi;
}

void set_render_backend(RenderBackendKind kind) {
  g_has_override = true;
  g_override = kind;
}

void clear_render_backend_override() {
  g_has_override = false;
}

bool apply_render_backend_command(const char* command_id) {
  if (!command_id || command_id[0] == '\0') {
    return false;
  }
  if (std::strcmp(command_id, kCmdViewBackendMapLibre) == 0) {
    set_render_backend(RenderBackendKind::kTrackAMapLibre);
    return true;
  }
  if (std::strcmp(command_id, kCmdViewBackendRhi) == 0) {
    set_render_backend(RenderBackendKind::kTrackBRhi);
    return true;
  }
  return false;
}

const char* render_backend_name(RenderBackendKind kind) {
  return kind == RenderBackendKind::kTrackAMapLibre ? "track_a" : "track_b";
}

bool maplibre_runtime_compiled() {
  return detail::maplibre_runtime_compiled_impl();
}

bool maplibre_map_linked() {
  return detail::maplibre_map_linked_impl();
}

bool paint_map_frame(detail::PresentTarget* present,
                     const MapPaintRequest& req) {
  if (!present) {
    return false;
  }
  if (req.kind == content::ViewKind::kScene3d) {
    return false;
  }
  return paint_track_a_basemap(present, req);
}

}  // namespace gpu
