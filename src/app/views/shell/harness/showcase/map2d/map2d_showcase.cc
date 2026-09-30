// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/map2d_showcase.h"

#include <windows.h>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "plugin/product/orthogrid/detail/boundary_solve.h"
#include "render/rhi/rhi.h"
#include "tool/draft/draft.h"
#include "ui/views/map/map_viewport.h"

#include <algorithm>
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

constexpr int kMap2dShowcaseW = 640;
constexpr int kMap2dShowcaseH = 480;

void showcase_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  if (!app::detail::exe_sidecar_path(path, MAX_PATH,
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
  if (browser.scene3d()) {
    browser.scene3d()->abandon_mesh();
  }
  if (ui::views::MapViewport* m = browser.map_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_data_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_scene_viewport()) {
    m->detach();
  }
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

void try_use_maplibre_carto(app::Browser& browser) {
  // Prefer default carto (land/water/river/road/admin) over china_city.style.json.
  // The sample style paints every line #1565c0 at width 2, which reads as a
  // blue scribble over cream ?not MapLibre/Baidu aligned.
  if (!browser.document()) {
    return;
  }
  browser.document()->clear_style_document();
  showcase_mark("style-carto-default");
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

// Unit-square gridbnd (nx=ny=17) ? Dirichlet Laplace ? line mesh in MapScene.
bool load_orthogrid_mesh(app::Browser& browser) {
  if (!browser.document()) {
    return false;
  }
  char tmp[MAX_PATH] = {};
  if (GetTempPathA(MAX_PATH, tmp) == 0) {
    return false;
  }
  char path[MAX_PATH] = {};
  if (sprintf_s(path, "%sorthogrid_showcase_%lu.gridbnd", tmp,
                static_cast<unsigned long>(GetCurrentProcessId())) <= 0) {
    return false;
  }
  {
    std::ofstream out(path);
    if (!out) {
      return false;
    }
    constexpr int n = 17;
    out << "gridbnd:\n" << n << " " << n << "\n";
    out << "begin\nmain_begin\n";
    // Four curved boundaries (flag 0..3) around the unit square.
    out << "3\n0 " << (n - 1) << " 0 0 1 1\n0,0\n0.5,0.04\n1,0\n";
    out << "3\n0 " << (n - 1) << " " << (n - 1) << " 1 1 1\n1,0\n0.96,0.5\n1,1\n";
    out << "3\n" << (n - 1) << " 0 " << (n - 1) << " 2 1 1\n1,1\n0.5,0.96\n0,1\n";
    out << "3\n" << (n - 1) << " 0 0 3 1 1\n0,1\n0.04,0.5\n0,0\n";
    out << "main_end\nend\n";
  }
  const plugin::detail::BoundarySolve solved =
      plugin::detail::solve_grid_boundary_file(path);
  DeleteFileA(path);
  if (!solved.ok || solved.nx < 3 || solved.ny < 3 ||
      solved.xs.size() != static_cast<size_t>(solved.nx * solved.ny)) {
    return false;
  }

  browser.document()->clear();
  // Drop china_city StyleDocument from shell seed — otherwise layout keys on
  // layer title ("orthogrid") and style layers never match ? empty ocean BMP.
  browser.document()->clear_style_document();
  // ViewFrame::fit_extent early-returns when there are no polygons — plant an
  // MBR footprint so the unit-square mesh is framed into the export BMP.
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
          // Match OGR ingest: map Y is -lat so layout's -p.y restores lon/lat.
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
    // Road gold reads on ocean wash; default "line" ? river blue is too soft.
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
  std::fprintf(stderr, "map2d-showcase mode=%s\n", name);
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
    try_use_maplibre_carto(browser);
  }

  // Frame ViewFrame to export pixels — not the live HWND client size.
  // fit_map_extent() uses Map Edit client (~2k wide); export_bmp would then
  // sample only the NW 640x480 of that pan (often ocean). Use full China
  // lon/lat (same as CHINA_BBOX / content overview) so Inner Mongolia is not
  // sliced by a flat 48N chord that reads as a "polyline" northern frontier.
  content::ViewFrame* frame = browser.view_frame();
  if (!frame) {
    std::fprintf(stderr, "map2d-showcase: ViewFrame missing\n");
    detach_maps(browser);
    return 57;
  }
  constexpr content::Extent2 kMainlandFraming{73.0, 18.0, 135.0, 54.0};
  if (mode == Map2dShowcaseMode::kOrthogrid) {
    // Explicit lon/lat framing (unit square). fit_extent alone has historically
    // left the ViewFrame on China framing when polygon MBR / Y-flip disagree.
    constexpr content::Extent2 kOrthogridFraming{0.0, 0.0, 1.0, 1.0};
    frame->apply_world_extent(kOrthogridFraming, kMap2dShowcaseW,
                              kMap2dShowcaseH);
    double minx = 0, miny = 0, maxx = 0, maxy = 0;
    if (browser.document()->compute_extent(&minx, &miny, &maxx, &maxy)) {
      std::fprintf(stderr,
                   "map2d-showcase: orthogrid extent=(%.3f,%.3f)-(%.3f,%.3f) "
                   "features=%zu scale=%g\n",
                   minx, miny, maxx, maxy, browser.document()->feature_count(),
                   frame->scale());
    }
  } else if (mode == Map2dShowcaseMode::kAlign ||
             (browser.document() && browser.document()->has_china_extent())) {
    frame->apply_world_extent(kMainlandFraming, kMap2dShowcaseW,
                              kMap2dShowcaseH);
  } else {
    frame->fit_extent(*browser.document(), kMap2dShowcaseW, kMap2dShowcaseH);
  }
  if (browser.map2d()) {
    browser.map2d()->invalidate_frame_cache();
  }
  app::pump_views_messages(100);
  showcase_mark("fit-ok");

  wchar_t bmp_w[MAX_PATH] = {};
  wchar_t file[64] = {};
  swprintf_s(file, L"map2d-showcase-%S.bmp", name);
  if (!app::detail::exe_sidecar_path(bmp_w, MAX_PATH, file)) {
    std::fprintf(stderr, "map2d-showcase: sidecar path failed\n");
    detach_maps(browser);
    return 56;
  }
  char bmp_a[MAX_PATH * 3] = {};
  WideCharToMultiByte(CP_UTF8, 0, bmp_w, -1, bmp_a, sizeof(bmp_a), nullptr,
                      nullptr);
  DeleteFileW(bmp_w);

  content::Map2dPresenter* map2d = browser.map2d();
  if (!map2d) {
    std::fprintf(stderr, "map2d-showcase: Map2dPresenter missing\n");
    detach_maps(browser);
    return 57;
  }
  map2d->invalidate_frame_cache();

  // Push a GDI overlay redraw so the MapEdit HWND matches export_bmp (requires
  // SMT_FORCE_GDI_MAP_OVERLAY from browser_main). ContentMapView alone can
  // leave the client rect as ocean even when software export has land.
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    if (pane->native_view() && IsWindow(pane->native_view())) {
      InvalidateRect(pane->native_view(), nullptr, FALSE);
      UpdateWindow(pane->native_view());
    }
  }
  app::pump_views_messages(200);
  showcase_mark("overlay-paint");

  // Optional FlyCube present smoke (HWND path). Capture still uses software
  // export so carto colors are channel-correct for map2d_shot_loop gates.
  if (want_gpu_present()) {
    showcase_mark("gpu-try");
    render::rhi::Device* device =
        render::rhi::create_device(render::rhi::preferred_gpu_backend());
    if (device) {
      render::rhi::DeviceDesc desc;
      desc.width = kMap2dShowcaseW;
      desc.height = kMap2dShowcaseH;
      if (ui::views::MapViewport* pane = browser.map_viewport()) {
        if (pane->native_view() && IsWindow(pane->native_view())) {
          desc.native_window = pane->native_view();
        }
      }
      if (device->initialize(desc)) {
        const bool ok =
            map2d->present_gpu(device, kMap2dShowcaseW, kMap2dShowcaseH);
        std::fprintf(stderr, "map2d-showcase: present_gpu=%d\n", ok ? 1 : 0);
        showcase_mark(ok ? "gpu-present-ok" : "gpu-present-fail");
        device->shutdown();
        // Intentionally leak Device* ?same FlyCube teardown policy as
        // atmosphere showcase / MapViewport.
      }
    }
  }

  if (!map2d->export_bmp(bmp_a, kMap2dShowcaseW, kMap2dShowcaseH)) {
    std::fprintf(stderr, "map2d-showcase: export_bmp failed\n");
    showcase_mark("bmp-fail");
    detach_maps(browser);
    return 56;
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
  showcase_mark("pass");
  std::fprintf(stderr, "map2d-showcase: PASS mode=%s\n", name);
  detach_maps(browser);
  return 0;
}

}  // namespace

namespace app {

int run_map2d_showcase(Browser& browser, Map2dShowcaseMode mode) {
  return run_map2d_showcase_impl(browser, mode);
}

}  // namespace app
