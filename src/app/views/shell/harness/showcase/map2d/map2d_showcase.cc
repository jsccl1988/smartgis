// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/map2d_showcase.h"

#include <windows.h>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"
#include "content/browser/present/map2d/gpu/map2d_gpu_present.h"
#include "plugin/product/orthogrid/commands.h"
#include "plugin/product/orthogrid/detail/boundary_solve.h"
#include "render/rhi/rhi.h"
#include "tool/draft/draft.h"
#include "ui/views/map/map_viewport.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using app::Map2dShowcaseMode;
using app::map2d_showcase_name;

constexpr int kMap2dShowcaseDefaultW = 640;
constexpr int kMap2dShowcaseDefaultH = 480;

// Optional SMT_MAP2D_SHOWCASE_W / SMT_MAP2D_SHOWCASE_H (matrix uses 1280x720).
void showcase_pixel_size(int* out_w, int* out_h) {
  int w = kMap2dShowcaseDefaultW;
  int h = kMap2dShowcaseDefaultH;
  if (const char* ew = std::getenv("SMT_MAP2D_SHOWCASE_W");
      ew && ew[0] != '\0') {
    const int n = std::atoi(ew);
    if (n >= 320 && n <= 3840) {
      w = n;
    }
  }
  if (const char* eh = std::getenv("SMT_MAP2D_SHOWCASE_H");
      eh && eh[0] != '\0') {
    const int n = std::atoi(eh);
    if (n >= 240 && n <= 2160) {
      h = n;
    }
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
}

void showcase_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  if (!app::detail::exe_capture_path(path, MAX_PATH,
                                     L"map2d-showcase-mark.txt")) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
  }
}

void detach_maps(app::Browser& browser) {
  app::detail::detach_maps(browser);
}

bool try_open_china_sample(app::Browser& browser) {
  if (browser.document() && browser.document()->layer_count() > 0 &&
      browser.document()->feature_count() >= 3) {
    return true;
  }
  wchar_t sample_w[MAX_PATH] = {};
  if (!app::detail::exe_dir_with_slash(sample_w, MAX_PATH)) {
    return false;
  }
  char sample_a[MAX_PATH] = {};
  const wchar_t* candidates[] = {L"..\\data\\china_city.gpkg",
                                 L"..\\data\\china_city.geojson",
                                 L"..\\data\\china_plp.geojson",
                                 L"data\\china_city.gpkg",
                                 L"data\\china_city.geojson",
                                 L"data\\china_plp.geojson",
                                 L"china_city.gpkg",
                                 L"china_city.geojson",
                                 L"china_plp.geojson"};
  for (const wchar_t* name : candidates) {
    wchar_t china_w[MAX_PATH] = {};
    if (wcscpy_s(china_w, sample_w) != 0 || wcscat_s(china_w, name) != 0) {
      continue;
    }
    if (GetFileAttributesW(china_w) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    WideCharToMultiByte(CP_UTF8, 0, china_w, -1, sample_a, MAX_PATH, nullptr,
                        nullptr);
    if (browser.document()->open_path(sample_a) &&
        browser.document()->last_open_was_ogr() &&
        browser.document()->feature_count() >= 3) {
      return true;
    }
  }
  return false;
}

bool try_path_candidates(const wchar_t* const* rels, size_t count,
                         char* out_utf8, size_t out_cap) {
  wchar_t base[MAX_PATH] = {};
  if (!app::detail::exe_dir_with_slash(base, MAX_PATH)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    wchar_t full[MAX_PATH] = {};
    if (wcscpy_s(full, base) != 0 || wcscat_s(full, rels[i]) != 0) {
      continue;
    }
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, full, -1, out_utf8,
                            static_cast<int>(out_cap), nullptr, nullptr) <= 0) {
      continue;
    }
    return true;
  }
  return false;
}

bool try_load_align_style(app::Browser& browser) {
  if (!browser.document()) {
    return false;
  }
  char path_a[MAX_PATH * 3] = {};
  const wchar_t* candidates[] = {
      L"maplibre\\example\\style_align.json",
      L"..\\maplibre\\example\\style_align.json",
      L"..\\..\\third_party\\maplibre\\example\\style_align.json",
      L"third_party\\maplibre\\example\\style_align.json",
  };
  if (!try_path_candidates(candidates, std::size(candidates), path_a,
                           sizeof(path_a))) {
    return false;
  }
  if (!browser.document()->load_style_path(path_a)) {
    return false;
  }
  showcase_mark("style-align");
  return true;
}

bool bmp_has_visible_signal(const char* path, int* out_w, int* out_h) {
  if (!path) {
    return false;
  }
  FILE* in = nullptr;
  if (fopen_s(&in, path, "rb") != 0 || !in) {
    return false;
  }
  BITMAPFILEHEADER fh{};
  BITMAPINFOHEADER bi{};
  if (std::fread(&fh, sizeof(fh), 1, in) != 1 ||
      std::fread(&bi, sizeof(bi), 1, in) != 1 || fh.bfType != 0x4D42) {
    std::fclose(in);
    return false;
  }
  const int w = bi.biWidth;
  const int h = bi.biHeight < 0 ? -bi.biHeight : bi.biHeight;
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  if (w < 320 || h < 240 ||
      (bi.biBitCount != 24 && bi.biBitCount != 32)) {
    std::fclose(in);
    return false;
  }
  const int bpp = bi.biBitCount / 8;
  const int stride = ((w * bi.biBitCount + 31) / 32) * 4;
  std::vector<unsigned char> pixels(static_cast<size_t>(stride) *
                                    static_cast<size_t>(h));
  if (std::fseek(in, static_cast<long>(fh.bfOffBits), SEEK_SET) != 0 ||
      std::fread(pixels.data(), 1, pixels.size(), in) != pixels.size()) {
    std::fclose(in);
    return false;
  }
  std::fclose(in);

  int lit = 0;
  int samples = 0;
  const int step_x = (std::max)(1, w / 32);
  const int step_y = (std::max)(1, h / 24);
  for (int y = 0; y < h; y += step_y) {
    const unsigned char* row =
        pixels.data() + static_cast<size_t>(y) * stride;
    for (int x = 0; x < w; x += step_x) {
      const unsigned char b = row[x * bpp + 0];
      const unsigned char g = row[x * bpp + 1];
      const unsigned char r = row[x * bpp + 2];
      ++samples;
      if (static_cast<int>(r) + g + b > 24) {
        ++lit;
      }
    }
  }
  return samples > 0 && lit * 20 >= samples;
}

bool want_gpu_present() {
  if (const char* env = std::getenv("SMT_MAP2D_SHOWCASE_GPU")) {
    return env[0] == '1' && env[1] == '\0';
  }
  return false;
}

void log_map2d_phase_sample(const char* tag) {
  const content::Map2dPhaseSample s = content::map2d_last_phase_sample();
  std::fprintf(stderr,
               "map2d-showcase: %s layout_ms=%lld hillshade_ms=%lld "
               "software_paint_ms=%lld paint_ms=%lld bmp_io_ms=%lld "
               "gpu_upload_ms=%lld gpu_present_ms=%lld\n",
               tag, static_cast<long long>(s.layout_ms),
               static_cast<long long>(s.hillshade_ms),
               static_cast<long long>(s.software_paint_ms),
               static_cast<long long>(s.software_paint_ms),
               static_cast<long long>(s.bmp_io_ms),
               static_cast<long long>(s.gpu_upload_ms),
               static_cast<long long>(s.gpu_present_ms));
}

bool want_export_reuse() {
  if (const char* env = std::getenv("SMT_MAP2D_EXPORT_REUSE")) {
    return env[0] == '1' && env[1] == '\0';
  }
  return false;
}

// Create one Device for cold+warm samples (showcase size). Prefer a dedicated
// init at export pixels so HWND client size does not force swapchain churn.
render::rhi::Device* acquire_showcase_gpu_device(app::Browser& browser,
                                                 int showcase_w, int showcase_h,
                                                 bool* out_owned) {
  *out_owned = false;
  // Reuse MapViewport FlyCube device only when already warm at matching size.
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    if (pane->attach_mode() == ui::views::MapViewport::AttachMode::kFlyCube &&
        pane->rhi_device()) {
      auto* device = static_cast<render::rhi::Device*>(pane->rhi_device());
      // Prefer dedicated device when viewport size ≠ showcase export size.
      RECT rc = {};
      if (pane->native_view() && IsWindow(pane->native_view())) {
        GetClientRect(pane->native_view(), &rc);
      }
      const int cw = rc.right > 0 ? rc.right : 0;
      const int ch = rc.bottom > 0 ? rc.bottom : 0;
      if (cw == showcase_w && ch == showcase_h) {
        return device;
      }
    }
  }
  render::rhi::Device* device =
      render::rhi::create_device(render::rhi::preferred_gpu_backend());
  if (!device) {
    return nullptr;
  }
  render::rhi::DeviceDesc desc;
  desc.width = showcase_w;
  desc.height = showcase_h;
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    if (pane->native_view() && IsWindow(pane->native_view())) {
      desc.native_window = pane->native_view();
    }
  }
  if (!device->initialize(desc)) {
    device->shutdown();
    // Intentionally leak Device* — FlyCube teardown policy.
    return nullptr;
  }
  *out_owned = true;
  return device;
}

// Coastal bay gridbnd (nx=ny=33) ? Laplace + Thompson ? heat mesh in MapScene.
bool load_orthogrid_mesh(app::Browser& browser) {
  if (!browser.document()) {
    return false;
  }
  char bnd_path[MAX_PATH] = {};
  {
    wchar_t base[MAX_PATH] = {};
    if (!app::detail::exe_dir_with_slash(base, MAX_PATH)) {
      return false;
    }
    const wchar_t* rels[] = {L"..\\data\\plugin\\orthogrid_sample.gridbnd",
                             L"data\\plugin\\orthogrid_sample.gridbnd"};
    bool found = false;
    for (const wchar_t* rel : rels) {
      wchar_t full[MAX_PATH] = {};
      if (wcscpy_s(full, base) != 0 || wcscat_s(full, rel) != 0) {
        continue;
      }
      if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
        continue;
      }
      if (WideCharToMultiByte(CP_UTF8, 0, full, -1, bnd_path,
                              static_cast<int>(sizeof(bnd_path)), nullptr,
                              nullptr) <= 0) {
        continue;
      }
      found = true;
      break;
    }
    if (!found) {
      std::fprintf(stderr, "map2d-showcase: missing orthogrid_sample.gridbnd\n");
      return false;
    }
  }
  // Thompson steps help orthogonality on the irregular coastal Dirichlet.
  constexpr int kEllipticIters = 4;
  const plugin::detail::BoundarySolve solved =
      plugin::detail::solve_grid_boundary_file(bnd_path, kEllipticIters);
  if (!solved.ok || solved.nx < 3 || solved.ny < 3 ||
      solved.xs.size() != static_cast<size_t>(solved.nx * solved.ny)) {
    return false;
  }

  plugin::OrthogridMeshCommit commit;
  commit.nx = solved.nx;
  commit.ny = solved.ny;
  commit.xs = solved.xs.data();
  commit.ys = solved.ys.data();
  if (!solved.cell_orth.empty()) {
    commit.cell_orth = solved.cell_orth.data();
  }
  if (!solved.raster_orth.empty() && solved.raster_w > 0 &&
      solved.raster_h > 0) {
    commit.raster_w = solved.raster_w;
    commit.raster_h = solved.raster_h;
    commit.raster_min_x = solved.raster_min_x;
    commit.raster_min_y = solved.raster_min_y;
    commit.raster_max_x = solved.raster_max_x;
    commit.raster_max_y = solved.raster_max_y;
    commit.raster_orth = solved.raster_orth.data();
  }
  if (plugin::publish_orthogrid_mesh(commit)) {
    return browser.document()->feature_count() >= 2;
  }

  // Fallback when mesh writer is unset (unit harness without Browser::init).
  browser.document()->clear();
  browser.document()->clear_style_document();
  if (!browser.document()->create_layer("orthogrid_extent", "Polygon")) {
    return false;
  }
  {
    tool::Draft extent;
    extent.kind = tool::DraftKind::kPolygon;
    extent.points = {{0, 0}, {1000, 0}, {1000, 1000}, {0, 1000}};
    browser.document()->append_from_draft(
        extent, "draw.polygon",
        [](int vx, int vy, double* map_x, double* map_y) {
          *map_x = static_cast<double>(vx) / 1000.0;
          *map_y = -static_cast<double>(vy) / 1000.0;
        });
  }
  if (!browser.document()->create_layer("orthogrid", "LineString")) {
    return false;
  }

  auto append_polyline = [&](const std::vector<std::pair<double, double>>& xy) {
    if (xy.size() < 2) {
      return false;
    }
    tool::Draft draft;
    draft.kind = tool::DraftKind::kLineString;
    draft.points.reserve(xy.size());
    for (size_t i = 0; i < xy.size(); ++i) {
      draft.points.push_back({static_cast<int32_t>(i), 0});
    }
    const content::FeatureId id = browser.document()->append_from_draft(
        draft, "draw.linestring",
        [&xy](int view_x, int, double* map_x, double* map_y) {
          const size_t i = static_cast<size_t>(view_x);
          if (i >= xy.size() || !map_x || !map_y) {
            return;
          }
          *map_x = xy[i].first;
          *map_y = -xy[i].second;
        });
    if (id.len != 0) {
      browser.document()->update_feature_field(
          content::MapScene::feature_token(id), "type", "highway");
    }
    return id.len != 0;
  };

  const int nx = solved.nx;
  const int ny = solved.ny;
  for (int j = 0; j < ny; ++j) {
    std::vector<std::pair<double, double>> row;
    row.reserve(static_cast<size_t>(nx));
    for (int i = 0; i < nx; ++i) {
      const size_t at = static_cast<size_t>(j * nx + i);
      row.emplace_back(solved.xs[at], solved.ys[at]);
    }
    if (!append_polyline(row)) {
      return false;
    }
  }
  for (int i = 0; i < nx; ++i) {
    std::vector<std::pair<double, double>> col;
    col.reserve(static_cast<size_t>(ny));
    for (int j = 0; j < ny; ++j) {
      const size_t at = static_cast<size_t>(j * nx + i);
      col.emplace_back(solved.xs[at], solved.ys[at]);
    }
    if (!append_polyline(col)) {
      return false;
    }
  }
  return browser.document()->feature_count() >= 2;
}

int run_map2d_showcase_impl(app::Browser& browser, Map2dShowcaseMode mode) {
  const char* name = map2d_showcase_name(mode);
  int showcase_w = kMap2dShowcaseDefaultW;
  int showcase_h = kMap2dShowcaseDefaultH;
  showcase_pixel_size(&showcase_w, &showcase_h);
  std::fprintf(stderr, "map2d-showcase mode=%s size=%dx%d\n", name, showcase_w,
               showcase_h);
  showcase_mark(name);

  if (mode != Map2dShowcaseMode::kChina && mode != Map2dShowcaseMode::kAlign &&
      mode != Map2dShowcaseMode::kOrthogrid) {
    std::fprintf(stderr, "map2d-showcase: unsupported mode\n");
    return 53;
  }

  browser.select_map_tab(0);
  showcase_mark("tab-map");
  app::pump_views_messages(400);
  showcase_mark("pumped");

  if (mode == Map2dShowcaseMode::kOrthogrid) {
    if (!load_orthogrid_mesh(browser)) {
      std::fprintf(stderr, "map2d-showcase: orthogrid mesh failed\n");
      detach_maps(browser);
      return 55;
    }
    showcase_mark("orthogrid-ok");
  } else if (mode == Map2dShowcaseMode::kAlign) {
    // Same china_city pack as --map2d-showcase=china / main-app Open.
    if (!try_open_china_sample(browser)) {
      if (browser.document()) {
        browser.document()->seed_default();
      }
    }
    if (!browser.document() || browser.document()->feature_count() < 3) {
      std::fprintf(stderr, "map2d-showcase: china sample open failed\n");
      detach_maps(browser);
      return 55;
    }
    showcase_mark("china-ok");
    if (!try_load_align_style(browser)) {
      std::fprintf(stderr, "map2d-showcase: style_align.json load failed\n");
      detach_maps(browser);
      return 55;
    }
  } else {
    if (!try_open_china_sample(browser)) {
      // Fall back to MapScene seed (china_city / stub).
      if (browser.document()) {
        browser.document()->seed_default();
      }
    }
    if (!browser.document() || browser.document()->feature_count() < 3) {
      std::fprintf(stderr, "map2d-showcase: china sample open failed\n");
      detach_maps(browser);
      return 55;
    }
    showcase_mark("china-ok");
    // Same carto clear as interactive fit_map_extent / seed_default.
    app::ensure_china_maplibre_carto(browser);
    showcase_mark("style-carto-default");
  }

  // Frame ViewFrame to export pixels � not the live HWND client size.
  // fit_map_extent() uses Map Edit client (~2k wide); export_bmp would then
  // sample only the NW 640x480 of that pan (often ocean).
  content::ViewFrame* frame = browser.view_frame();
  if (!frame) {
    std::fprintf(stderr, "map2d-showcase: ViewFrame missing\n");
    detach_maps(browser);
    return 57;
  }
  if (mode == Map2dShowcaseMode::kOrthogrid) {
    // Explicit lon/lat framing (unit square). fit_extent alone has historically
    // left the ViewFrame on China framing when polygon MBR / Y-flip disagree.
    constexpr content::Extent2 kOrthogridFraming{0.0, 0.0, 1.0, 1.0};
    frame->apply_world_extent(kOrthogridFraming, showcase_w, showcase_h);
    double minx = 0, miny = 0, maxx = 0, maxy = 0;
    if (browser.document()->compute_extent(&minx, &miny, &maxx, &maxy)) {
      std::fprintf(stderr,
                   "map2d-showcase: orthogrid extent=(%.3f,%.3f)-(%.3f,%.3f) "
                   "features=%zu scale=%g\n",
                   minx, miny, maxx, maxy, browser.document()->feature_count(),
                   frame->scale());
    }
    if (browser.map2d()) {
      browser.map2d()->invalidate_frame_cache();
    }
  } else {
    // Shared mainland framing with interactive China product defaults
    // (keep align StyleDocument � do not clear_style here).
    app::frame_china_map2d(browser, showcase_w, showcase_h);
  }
  app::pump_views_messages(100);
  showcase_mark("fit-ok");

  wchar_t bmp_w[MAX_PATH] = {};
  char leaf_a[64] = {};
  std::snprintf(leaf_a, sizeof(leaf_a), "map2d-showcase-%s.bmp", name);
  wchar_t leaf_w[64] = {};
  MultiByteToWideChar(CP_ACP, 0, leaf_a, -1, leaf_w, 64);
  if (!app::detail::exe_capture_path(bmp_w, MAX_PATH, leaf_w)) {
    std::fprintf(stderr, "map2d-showcase: sidecar path failed\n");
    detach_maps(browser);
    return 56;
  }
  // fopen_s in export_bmp expects ACP, not UTF-8 � keep ANSI sidecar.
  char bmp_a[MAX_PATH] = {};
  if (!app::detail::exe_capture_path_a(bmp_a, MAX_PATH, leaf_a)) {
    std::fprintf(stderr, "map2d-showcase: sidecar path_a failed\n");
    detach_maps(browser);
    return 56;
  }
  std::fprintf(stderr, "map2d-showcase: bmp path=%s\n", bmp_a);
  showcase_mark("bmp-path");
  DeleteFileW(bmp_w);
  showcase_mark("bmp-cleared");

  content::Map2dPresenter* map2d = browser.map2d();
  if (!map2d) {
    std::fprintf(stderr, "map2d-showcase: Map2dPresenter missing\n");
    detach_maps(browser);
    return 57;
  }
  // frame_china_map2d / orthogrid already invalidated when size/extent changed.
  // Do not invalidate again immediately before timed present — that forces a
  // cold layout+upload into the present_gpu wall clock.
  showcase_mark("cache-ready");

  // Capture uses software export_bmp — do NOT UpdateWindow here. Sync GDI
  // paint through the HWND has AVd in Map2dSoftwarePainter / ContentMapView
  // under parallel harness (mark stops at bmp-path). Async InvalidateRect is
  // enough so the live HWND may refresh; BMP does not depend on it.
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    if (pane->native_view() && IsWindow(pane->native_view())) {
      InvalidateRect(pane->native_view(), nullptr, FALSE);
    }
  }
  app::pump_views_messages(50);
  showcase_mark("overlay-paint");

  // Warm layout+hillshade AFTER HWND pump: a live paint at client size would
  // otherwise rebuild MapFrame at ~2k and clobber the showcase 1280x720 cache.
  if (!map2d->frame_cache().ensure_full(
          static_cast<uint32_t>(showcase_w),
          static_cast<uint32_t>(showcase_h))) {
    std::fprintf(stderr, "map2d-showcase: ensure_full layout failed\n");
    detach_maps(browser);
    return 57;
  }
  showcase_mark("layout-warm");

  // Optional FlyCube present smoke (HWND path = src/render RHI 2D). Capture
  // still uses software export so carto colors are channel-correct for gates.
  // Reuse one Device across cold + warm samples; report both separately.
  long long present_gpu_cold_ms = -1;
  long long present_gpu_warm_ms = -1;
  if (want_gpu_present()) {
    showcase_mark("gpu-try");
    bool owned_device = false;
    render::rhi::Device* device =
        acquire_showcase_gpu_device(browser, showcase_w, showcase_h,
                                   &owned_device);
    if (device) {
      content::reset_map2d_phase_sample();
      const auto t_cold = std::chrono::steady_clock::now();
      const bool ok_cold =
          map2d->present_gpu(device, showcase_w, showcase_h);
      present_gpu_cold_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - t_cold)
              .count();
      std::fprintf(stderr,
                   "map2d-showcase: present_gpu=%d present_gpu_ms=%lld "
                   "present_gpu_cold_ms=%lld\n",
                   ok_cold ? 1 : 0, present_gpu_cold_ms, present_gpu_cold_ms);
      log_map2d_phase_sample("phase_cold_present");
      showcase_mark(ok_cold ? "gpu-present-ok" : "gpu-present-fail");

      content::reset_map2d_phase_sample();
      const auto t_warm = std::chrono::steady_clock::now();
      const bool ok_warm =
          map2d->present_gpu(device, showcase_w, showcase_h);
      present_gpu_warm_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - t_warm)
              .count();
      std::fprintf(stderr,
                   "map2d-showcase: present_gpu_warm=%d present_gpu_warm_ms=%lld\n",
                   ok_warm ? 1 : 0, present_gpu_warm_ms);
      log_map2d_phase_sample("phase_warm_present");
      showcase_mark(ok_warm ? "gpu-warm-ok" : "gpu-warm-fail");

      if (owned_device) {
        device->shutdown();
        // Intentionally leak Device* — same FlyCube teardown policy as
        // atmosphere showcase / MapViewport.
      }
    }
  }

  {
    // Bench: SMT_MAP2D_EXPORT_REUSE=1 warms present-cache off-clock, then the
    // timed export reports paint_ms (blit) vs export_ms (paint + bmp_io).
    if (want_export_reuse()) {
      showcase_mark("export-warm");
      char warm_a[MAX_PATH] = {};
      if (app::detail::exe_capture_path_a(warm_a, MAX_PATH,
                                          "map2d-showcase-export-warm.bmp")) {
        (void)map2d->export_bmp(warm_a, showcase_w, showcase_h);
      }
    }
    content::reset_map2d_phase_sample();
    const auto t0 = std::chrono::steady_clock::now();
    if (!map2d->export_bmp(bmp_a, showcase_w, showcase_h)) {
      std::fprintf(stderr, "map2d-showcase: export_bmp failed\n");
      showcase_mark("bmp-fail");
      detach_maps(browser);
      return 56;
    }
    const long long export_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0)
            .count();
    const content::Map2dPhaseSample export_ph =
        content::map2d_last_phase_sample();
    std::fprintf(stderr,
                 "map2d-showcase: export_ms=%lld paint_ms=%lld bmp_io_ms=%lld\n",
                 export_ms,
                 static_cast<long long>(export_ph.software_paint_ms),
                 static_cast<long long>(export_ph.bmp_io_ms));
    log_map2d_phase_sample("phase_export");
  }
  showcase_mark("bmp-wrote");

  int bw = 0;
  int bh = 0;
  if (!bmp_has_visible_signal(bmp_a, &bw, &bh)) {
    std::fprintf(stderr, "map2d-showcase: BMP lacks visible signal (%dx%d)\n",
                 bw, bh);
    showcase_mark("bmp-black");
    detach_maps(browser);
    return 54;
  }
  std::fprintf(stderr, "map2d-showcase: wrote %s (%dx%d)\n", bmp_a, bw, bh);
  showcase_mark("bmp-ok");

  // Durable copy: multi-agent harness loops often DeleteFile the canonical
  // leaf between bmp-ok and human inspection. Keep a sibling that loops do
  // not target.
  {
    wchar_t keep_w[MAX_PATH] = {};
    char keep_leaf[80] = {};
    std::snprintf(keep_leaf, sizeof(keep_leaf),
                  "map2d-showcase-%s.keep.bmp", name);
    wchar_t keep_leaf_w[80] = {};
    MultiByteToWideChar(CP_ACP, 0, keep_leaf, -1, keep_leaf_w, 80);
    if (app::detail::exe_capture_path(keep_w, MAX_PATH, keep_leaf_w)) {
      if (CopyFileW(bmp_w, keep_w, FALSE)) {
        showcase_mark("bmp-keep");
      }
    }
  }

  // Optional FPS bench: keep maps live, request presents, sample HUD FPS.
  // SMT_MAP2D_FPS_BENCH_MS=3000 (default off). Writes map2d-fps-bench.txt.
  if (const char* bench_env = std::getenv("SMT_MAP2D_FPS_BENCH_MS")) {
    const int bench_ms = std::atoi(bench_env);
    if (bench_ms > 0) {
      showcase_mark("fps-bench");
      ui::views::MapViewport* pane = browser.map_viewport();
      float sum = 0.f;
      float peak = 0.f;
      int samples = 0;
      const uint64_t builds0 = map2d->layout_build_count();
      content::reset_map2d_gpu_present_profile();
      // Warm past dual-speed settle (~200ms) so StaticReuse dominates samples.
      if (pane) {
        pane->invalidate_native();
        app::pump_views_messages(350);
      }
      const DWORD t0 = GetTickCount();
      while (static_cast<int>(GetTickCount() - t0) < bench_ms) {
        if (pane) {
          pane->invalidate_native();
          // Avoid sync_identity_chrome � it churns shell overlay generation.
          const float fps = pane->hud_fps();
          // Skip first 250ms of samples (settle + first StaticReuse).
          const int elapsed = static_cast<int>(GetTickCount() - t0);
          if (fps > 0.f && elapsed >= 250) {
            sum += fps;
            if (fps > peak) {
              peak = fps;
            }
            ++samples;
          }
        }
        app::pump_views_messages(16);
      }
      const float mean = samples > 0 ? (sum / static_cast<float>(samples)) : 0.f;
      const uint64_t builds_delta = map2d->layout_build_count() - builds0;
      const content::Map2dGpuPresentProfile prof =
          content::map2d_gpu_present_profile();
      const uint64_t present_n = prof.skip + prof.full;
      const float skip_pct =
          present_n > 0
              ? (100.f * static_cast<float>(prof.skip) /
                 static_cast<float>(present_n))
              : 0.f;
      std::fprintf(stderr,
                   "map2d-showcase: fps_bench ms=%d samples=%d mean=%.2f "
                   "peak=%.2f layout_builds_delta=%llu total=%llu "
                   "gpu_skip=%llu gpu_full=%llu skip_pct=%.1f "
                   "act_r/i/s/st=%llu/%llu/%llu/%llu\n",
                   bench_ms, samples, mean, peak,
                   static_cast<unsigned long long>(builds_delta),
                   static_cast<unsigned long long>(
                       map2d->layout_build_count()),
                   static_cast<unsigned long long>(prof.skip),
                   static_cast<unsigned long long>(prof.full), skip_pct,
                   static_cast<unsigned long long>(prof.action_rebuild),
                   static_cast<unsigned long long>(prof.action_interactive),
                   static_cast<unsigned long long>(prof.action_settle),
                   static_cast<unsigned long long>(prof.action_static));
      wchar_t bench_w[MAX_PATH] = {};
      if (app::detail::exe_capture_path(bench_w, MAX_PATH,
                                        L"map2d-fps-bench.txt")) {
        FILE* bf = nullptr;
        if (_wfopen_s(&bf, bench_w, L"w") == 0 && bf) {
          std::fprintf(bf,
                       "mean_fps=%.3f\npeak_fps=%.3f\nsamples=%d\n"
                       "bench_ms=%d\nlayout_builds=%llu\n"
                       "layout_builds_delta=%llu\n"
                       "gpu_skip=%llu\ngpu_full=%llu\nskip_pct=%.1f\n"
                       "action_rebuild=%llu\naction_interactive=%llu\n"
                       "action_settle=%llu\naction_static=%llu\n",
                       mean, peak, samples, bench_ms,
                       static_cast<unsigned long long>(
                           map2d->layout_build_count()),
                       static_cast<unsigned long long>(builds_delta),
                       static_cast<unsigned long long>(prof.skip),
                       static_cast<unsigned long long>(prof.full), skip_pct,
                       static_cast<unsigned long long>(prof.action_rebuild),
                       static_cast<unsigned long long>(
                           prof.action_interactive),
                       static_cast<unsigned long long>(prof.action_settle),
                       static_cast<unsigned long long>(prof.action_static));
          std::fclose(bf);
        }
      }
      showcase_mark("fps-bench-done");
    }
  }

  showcase_mark("pass");
  std::fprintf(stderr, "map2d-showcase: PASS mode=%s\n", name);
  // Timers only — detach_maps races TerminateProcess and surfaces as -1 after
  // a green BMP (peer browse / atmosphere).
  app::detail::stop_map_present_timers(browser);
  app::pump_views_messages(100);
  return 0;
}

}  // namespace

namespace app {

int map2d_showcase_body(Browser& browser, Map2dShowcaseMode mode) {
  return run_map2d_showcase_impl(browser, mode);
}

int run_map2d_showcase(Browser& browser, Map2dShowcaseMode mode) {
  // Prefer the C++ body. The thin map2d.*.il → map2d_run path has hung /
  // surfaced EXIT=-1 after Browser::show with no marks/BMP (ANTLR resolve or
  // CapabilityHost fill). Body writes map2d-showcase-mark.txt + BMP directly.
  // Optional IL remains for interactive / SMT_UI_INTERACT_SCRIPT overrides.
  if (const char* force_il = std::getenv("SMT_MAP2D_SHOWCASE_IL");
      force_il && force_il[0] == '1' && force_il[1] == '\0') {
    const char* suite = nullptr;
    switch (mode) {
      case Map2dShowcaseMode::kChina:
        suite = "map2d.china";
        break;
      case Map2dShowcaseMode::kAlign:
        suite = "map2d.align";
        break;
      case Map2dShowcaseMode::kOrthogrid:
        suite = "map2d.orthogrid";
        break;
      case Map2dShowcaseMode::kNone:
        break;
    }
    if (suite &&
        try_run_suite_script(browser, suite, detail::kMap2dShowcaseMarkLeaf,
                             /*clear_marks=*/true)) {
      return 0;
    }
  }
  return map2d_showcase_body(browser, mode);
}

}  // namespace app
