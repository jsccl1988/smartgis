// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/present.h"
#include "gpu/render_backend.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void clear_backend_env() {
  SetEnvironmentVariableA("SMT_MAP_BACKEND", nullptr);
}

}  // namespace

int main() {
  clear_backend_env();
  expect(gpu::select_render_backend() == gpu::RenderBackendKind::kTrackBRhi,
         "default backend is Track B");
  expect(std::strcmp(gpu::render_backend_name(gpu::RenderBackendKind::kTrackBRhi),
                     "track_b") == 0,
         "track_b name");

  SetEnvironmentVariableA("SMT_MAP_BACKEND", "a");
  expect(gpu::select_render_backend() == gpu::RenderBackendKind::kTrackAMapLibre,
         "SMT_MAP_BACKEND=a selects Track A");
  expect(std::strcmp(gpu::render_backend_name(
                         gpu::RenderBackendKind::kTrackAMapLibre),
                     "track_a") == 0,
         "track_a name");
  clear_backend_env();

  if (gpu::maplibre_runtime_compiled()) {
    std::fprintf(stdout, "smt_enable_maplibre: runtime TU compiled\n");
  } else {
    expect(!gpu::maplibre_map_linked(), "no mln::Map without pin+link");
  }
  expect(!gpu::maplibre_map_linked(), "pin without lib is not mln::Map");

  gpu::detail::PresentTarget present;
  HANDLE self = GetCurrentProcess();
  expect(present.resize(16, 16, content::PresentMode::kSoftwareDib, self),
         "dib resize");

  const char* k_style =
      "{"
      "\"version\":8,"
      "\"name\":\"track-a\","
      "\"layers\":["
      "{"
      "\"id\":\"bg\","
      "\"type\":\"background\","
      "\"paint\":{\"background-color\":\"#224466\"}"
      "}"
      "]"
      "}";

  gpu::MapPaintRequest req;
  req.kind = content::ViewKind::kMapEdit;
  req.style_json = k_style;
  expect(gpu::paint_map_frame(&present, req), "track-a background paint");

  std::vector<uint8_t> px(16u * 16u * 4u, 0);
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra");
  // #224466 → BGRA 66,44,22,FF
  expect(px[0] == 0x66 && px[1] == 0x44 && px[2] == 0x22 && px[3] == 0xFF,
         "background pixel BGRA");

  const uint8_t tile_b = 0x10;
  const uint8_t tile_g = 0xE0;
  const uint8_t tile_r = 0x30;
  req.tile_url_template = "http://127.0.0.1/{z}/{x}/{y}.png";
  req.fetch = [&](const std::string& url) {
    (void)url;
    gpu::TileFetchResult res;
    res.ok = true;
    // Four-byte BGRA solid tile (adapter RAW shortcut; not a PNG).
    res.body.assign(1, static_cast<char>(tile_b));
    res.body.push_back(static_cast<char>(tile_g));
    res.body.push_back(static_cast<char>(tile_r));
    res.body.push_back(static_cast<char>(0xFF));
    return res;
  };
  expect(gpu::paint_map_frame(&present, req), "track-a raster paint");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra after raster");
  expect(px[0] == tile_b && px[1] == tile_g && px[2] == tile_r && px[3] == 0xFF,
         "raster covers sample pixel");

  if (g_fails != 0) {
    std::fprintf(stderr, "render_backend_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "render_backend_test ok backend=track_b_default "
                       "track_a_pixels=1 mln_map=%d\n",
               gpu::maplibre_map_linked() ? 1 : 0);
  return 0;
}
