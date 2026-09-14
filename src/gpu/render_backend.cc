// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/render_backend.h"

#include "gpu/maplibre_adapter.h"
#include "gpu/maplibre_runtime.h"

#include <cstring>

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

}  // namespace

RenderBackendKind select_render_backend() {
  if (env_is_track_a()) {
    return RenderBackendKind::kTrackAMapLibre;
  }
  return RenderBackendKind::kTrackBRhi;
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
