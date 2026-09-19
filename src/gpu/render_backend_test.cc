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

gpu::TileFetchResult solid_tile(uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
  gpu::TileFetchResult res;
  res.ok = true;
  res.body.assign(1, static_cast<char>(b));
  res.body.push_back(static_cast<char>(g));
  res.body.push_back(static_cast<char>(r));
  res.body.push_back(static_cast<char>(a));
  return res;
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

  // Runtime override (chrome command / HostMsg). Beats SMT_MAP_BACKEND.
  gpu::set_render_backend(gpu::RenderBackendKind::kTrackAMapLibre);
  expect(gpu::select_render_backend() == gpu::RenderBackendKind::kTrackAMapLibre,
         "runtime override Track A");
  gpu::set_render_backend(gpu::RenderBackendKind::kTrackBRhi);
  expect(gpu::select_render_backend() == gpu::RenderBackendKind::kTrackBRhi,
         "runtime override Track B");
  SetEnvironmentVariableA("SMT_MAP_BACKEND", "a");
  gpu::set_render_backend(gpu::RenderBackendKind::kTrackBRhi);
  expect(gpu::select_render_backend() == gpu::RenderBackendKind::kTrackBRhi,
         "override beats SMT_MAP_BACKEND");
  gpu::clear_render_backend_override();
  expect(gpu::select_render_backend() == gpu::RenderBackendKind::kTrackAMapLibre,
         "cleared override uses env");
  clear_backend_env();
  expect(gpu::select_render_backend() == gpu::RenderBackendKind::kTrackBRhi,
         "default after clear");
  expect(gpu::apply_render_backend_command("view.backend.maplibre"),
         "command maplibre");
  expect(gpu::select_render_backend() == gpu::RenderBackendKind::kTrackAMapLibre,
         "command sets Track A");
  expect(gpu::apply_render_backend_command("view.backend.rhi"), "command rhi");
  expect(gpu::select_render_backend() == gpu::RenderBackendKind::kTrackBRhi,
         "command sets Track B");
  expect(!gpu::apply_render_backend_command("view.pan"), "unknown command");
  gpu::clear_render_backend_override();

  if (gpu::maplibre_runtime_compiled()) {
    std::fprintf(stdout, "smt_enable_maplibre: runtime TU compiled\n");
#if defined(SMT_HAS_MAPLIBRE_LIB)
    expect(gpu::maplibre_map_linked(), "flag+lib must report mln::Map linked");
#else
    expect(!gpu::maplibre_map_linked(), "compiled without lib is not mln::Map");
#endif
  } else {
    expect(!gpu::maplibre_map_linked(), "no mln::Map without pin+link");
  }

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

  const char* k_style_opacity =
      "{"
      "\"version\":8,"
      "\"layers\":[{"
      "\"id\":\"bg\","
      "\"type\":\"background\","
      "\"paint\":{"
      "\"background-color\":\"#224466\","
      "\"background-opacity\":0.5"
      "}"
      "}]"
      "}";
  req.style_json = k_style_opacity;
  req.tile_url_template = nullptr;
  req.tile_url_templates.clear();
  req.fetch = nullptr;
  expect(gpu::paint_map_frame(&present, req), "background-opacity paint");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra opacity");
  expect(px[0] == 0x66 && px[1] == 0x44 && px[2] == 0x22 && px[3] == 0x80,
         "background-opacity halves alpha");

  const char* k_style_multi =
      "{"
      "\"version\":8,"
      "\"layers\":["
      "{"
      "\"id\":\"bg\","
      "\"type\":\"background\","
      "\"paint\":{\"background-color\":\"#000000\"}"
      "},"
      "{"
      "\"id\":\"r0\","
      "\"type\":\"raster\","
      "\"paint\":{\"raster-opacity\":1.0}"
      "},"
      "{"
      "\"id\":\"r1\","
      "\"type\":\"raster\","
      "\"paint\":{\"raster-opacity\":0.5}"
      "}"
      "]"
      "}";
  req.style_json = k_style_multi;
  req.tile_url_templates = {
      "http://127.0.0.1/a/{z}/{x}/{y}.png",
      "http://127.0.0.1/b/{z}/{x}/{y}.png",
  };
  req.tile_url_template = nullptr;
  req.fetch = [](const std::string& url) {
    if (url.find("/a/") != std::string::npos) {
      // Opaque red.
      return solid_tile(0x00, 0x00, 0xFF, 0xFF);
    }
    if (url.find("/b/") != std::string::npos) {
      // Opaque green, composited at raster-opacity 0.5.
      return solid_tile(0x00, 0xFF, 0x00, 0xFF);
    }
    gpu::TileFetchResult miss;
    miss.ok = false;
    return miss;
  };
  expect(gpu::paint_map_frame(&present, req), "multi-raster paint");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra multi");
  // src-over green@0.5 on red → B=0, G≈128, R≈128, A=255
  expect(px[0] == 0x00, "multi B");
  expect(px[1] >= 0x7E && px[1] <= 0x82, "multi G ~128");
  expect(px[2] >= 0x7E && px[2] <= 0x82, "multi R ~128");
  expect(px[3] == 0xFF, "multi A");

  // Offline: multi-raster style without fetch still paints background.
  req.fetch = nullptr;
  req.tile_url_templates.clear();
  expect(gpu::paint_map_frame(&present, req), "offline background only");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra offline");
  expect(px[0] == 0x00 && px[1] == 0x00 && px[2] == 0x00 && px[3] == 0xFF,
         "offline keeps background");

  // Legacy single template + RAW tile (adapter path when rasters requested).
  req.style_json = k_style;
  req.tile_url_template = "http://127.0.0.1/{z}/{x}/{y}.png";
  req.tile_url_templates.clear();
  const uint8_t tile_b = 0x10;
  const uint8_t tile_g = 0xE0;
  const uint8_t tile_r = 0x30;
  req.fetch = [&](const std::string& url) {
    (void)url;
    return solid_tile(tile_b, tile_g, tile_r, 0xFF);
  };
  expect(gpu::paint_map_frame(&present, req), "track-a raster paint");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra after raster");
  // Background-only style + template still composites (caller XYZ hook).
  expect(px[0] == tile_b && px[1] == tile_g && px[2] == tile_r &&
             px[3] == 0xFF,
         "template without raster layer covers");

  const char* k_style_one_raster =
      "{"
      "\"version\":8,"
      "\"layers\":["
      "{"
      "\"id\":\"bg\","
      "\"type\":\"background\","
      "\"paint\":{\"background-color\":\"#224466\"}"
      "},"
      "{"
      "\"id\":\"xyz\","
      "\"type\":\"raster\","
      "\"paint\":{\"raster-opacity\":0.5}"
      "}"
      "]"
      "}";
  req.style_json = k_style_one_raster;
  req.fetch = [&](const std::string& url) {
    (void)url;
    // Opaque white over #224466 at 0.5.
    return solid_tile(0xFF, 0xFF, 0xFF, 0xFF);
  };
  expect(gpu::paint_map_frame(&present, req), "single raster layer paint");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra one raster");
  // src-over white@0.5 on #224466 → ~B 0xB3, G 0xA2, R 0x90, A FF
  expect(px[0] >= 0xB0 && px[0] <= 0xB6, "raster-opacity B");
  expect(px[1] >= 0x9F && px[1] <= 0xA5, "raster-opacity G");
  expect(px[2] >= 0x8D && px[2] <= 0x93, "raster-opacity R");
  expect(px[3] == 0xFF, "raster-opacity A");

  // Style `sources` + raster layer.source — no request templates required.
  const char* k_style_sources =
      "{"
      "\"version\":8,"
      "\"sources\":{"
      "\"basemap\":{"
      "\"type\":\"raster\","
      "\"tiles\":[\"http://127.0.0.1/src/{z}/{x}/{y}.png\"],"
      "\"tileSize\":256"
      "}"
      "},"
      "\"layers\":["
      "{"
      "\"id\":\"bg\","
      "\"type\":\"background\","
      "\"paint\":{\"background-color\":\"#112233\"}"
      "},"
      "{"
      "\"id\":\"xyz\","
      "\"type\":\"raster\","
      "\"source\":\"basemap\","
      "\"paint\":{\"raster-opacity\":1.0}"
      "}"
      "]"
      "}";
  req.style_json = k_style_sources;
  req.tile_url_template = nullptr;
  req.tile_url_templates.clear();
  req.fetch = [](const std::string& url) {
    if (url.find("/src/") != std::string::npos) {
      return solid_tile(0x20, 0x40, 0x80, 0xFF);
    }
    gpu::TileFetchResult miss;
    miss.ok = false;
    return miss;
  };
  expect(gpu::paint_map_frame(&present, req), "sources-bound raster paint");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra sources");
  expect(px[0] == 0x20 && px[1] == 0x40 && px[2] == 0x80 && px[3] == 0xFF,
         "sources layer.source covers");

  // Missing layer.source id: skip raster, keep background.
  const char* k_style_missing_source =
      "{"
      "\"version\":8,"
      "\"sources\":{"
      "\"basemap\":{"
      "\"type\":\"raster\","
      "\"tiles\":[\"http://127.0.0.1/src/{z}/{x}/{y}.png\"]"
      "}"
      "},"
      "\"layers\":["
      "{"
      "\"id\":\"bg\","
      "\"type\":\"background\","
      "\"paint\":{\"background-color\":\"#AABBCC\"}"
      "},"
      "{"
      "\"id\":\"xyz\","
      "\"type\":\"raster\","
      "\"source\":\"no-such-source\","
      "\"paint\":{\"raster-opacity\":1.0}"
      "}"
      "]"
      "}";
  req.style_json = k_style_missing_source;
  req.fetch = [](const std::string& url) {
    (void)url;
    return solid_tile(0xFF, 0x00, 0x00, 0xFF);
  };
  expect(gpu::paint_map_frame(&present, req), "missing source still paints");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra missing source");
  // #AABBCC → BGRA CC,BB,AA,FF
  expect(px[0] == 0xCC && px[1] == 0xBB && px[2] == 0xAA && px[3] == 0xFF,
         "missing source skips raster");

  // Viewport tiling: fake extent at z=1 spans four tiles, not only 0/0/0.
  const char* k_style_viewport =
      "{"
      "\"version\":8,"
      "\"layers\":["
      "{"
      "\"id\":\"bg\","
      "\"type\":\"background\","
      "\"paint\":{\"background-color\":\"#000000\"}"
      "},"
      "{"
      "\"id\":\"xyz\","
      "\"type\":\"raster\","
      "\"paint\":{\"raster-opacity\":1.0}"
      "}"
      "]"
      "}";
  req.style_json = k_style_viewport;
  req.tile_url_template = "http://127.0.0.1/vp/{z}/{x}/{y}.png";
  req.tile_url_templates.clear();
  // Small extent around the origin so z=1 yields x in {0,1} and y in {0,1}.
  req.extent = content::Extent2{-1000.0, -1000.0, 1000.0, 1000.0};
  req.zoom = 1;
  std::vector<std::string> fetched;
  req.fetch = [&](const std::string& url) {
    fetched.push_back(url);
    // Color encodes tile x in R and y in G (opaque).
    uint8_t tile_x = 0;
    uint8_t tile_y = 0;
    const size_t zpos = url.find("/vp/1/");
    if (zpos != std::string::npos) {
      int x = 0;
      int y = 0;
      if (std::sscanf(url.c_str() + zpos, "/vp/1/%d/%d.png", &x, &y) == 2) {
        tile_x = static_cast<uint8_t>(x & 0xFF);
        tile_y = static_cast<uint8_t>(y & 0xFF);
      }
    }
    return solid_tile(0x00, tile_y, tile_x, 0xFF);
  };
  expect(gpu::paint_map_frame(&present, req), "viewport mosaic paint");
  expect(fetched.size() >= 2, "viewport fetches more than one tile");
  bool saw_only_000 = true;
  bool saw_nonzero_xy = false;
  for (const auto& u : fetched) {
    if (u.find("/vp/0/0/0.png") == std::string::npos) {
      saw_only_000 = false;
    }
    if (u.find("/vp/1/") != std::string::npos &&
        u.find("/vp/1/0/0.png") == std::string::npos) {
      saw_nonzero_xy = true;
    }
  }
  expect(!saw_only_000, "viewport does not request only 0/0/0");
  expect(saw_nonzero_xy || fetched.size() >= 4, "viewport requests non-0/0 xy");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra viewport");
  // Left half of the 16px present samples x=0 (R=0); right half x=1 (R=1).
  expect(px[2] == 0x00, "viewport left R from x=0");
  const size_t right = (8u * 16u + 12u) * 4u;
  expect(px[right + 2] == 0x01, "viewport right R from x=1");

  // Degenerate extent keeps legacy single-tile 0/0/0.
  req.extent = content::Extent2{};
  req.zoom = -1;
  fetched.clear();
  req.fetch = [&](const std::string& url) {
    fetched.push_back(url);
    return solid_tile(0x11, 0x22, 0x33, 0xFF);
  };
  expect(gpu::paint_map_frame(&present, req), "fallback 0/0/0 paint");
  expect(fetched.size() == 1, "fallback fetches one tile");
  expect(fetched[0].find("/0/0/0.png") != std::string::npos,
         "fallback URL is z/x/y=0/0/0");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra fallback");
  expect(px[0] == 0x11 && px[1] == 0x22 && px[2] == 0x33 && px[3] == 0xFF,
         "fallback covers present");

  // Product net fetch: invalid URL fails fast (ok=false); paint keeps bg.
  auto net_fetch = gpu::make_net_tile_fetch();
  expect(static_cast<bool>(net_fetch), "make_net_tile_fetch returns callable");
  {
    const gpu::TileFetchResult bad = net_fetch("not-a-url");
    expect(!bad.ok && bad.body.empty(), "net fetch rejects invalid url");
  }
  req.style_json = k_style;
  req.extent = content::Extent2{};
  req.zoom = -1;
  // Template without scheme → HttpClient rejects immediately (no connect wait).
  req.tile_url_template = "not-a-url/{z}/{x}/{y}.png";
  req.tile_url_templates.clear();
  req.fetch = net_fetch;
  expect(gpu::paint_map_frame(&present, req),
         "net fetch miss still paints background");
  expect(present.copy_bgra(px.data(), px.size()), "copy_bgra after net miss");
  // Miss → background #224466 from k_style.
  expect(px[0] == 0x66 && px[1] == 0x44 && px[2] == 0x22 && px[3] == 0xFF,
         "net miss keeps background");

  if (g_fails != 0) {
    std::fprintf(stderr, "render_backend_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "render_backend_test ok backend=track_b_default "
                       "track_a_pixels=1 mln_map=%d\n",
               gpu::maplibre_map_linked() ? 1 : 0);
  return 0;
}
