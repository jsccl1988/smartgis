// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/self_test/self_test.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "app/views/shell/util/exe_sidecar_path.h"

#include <windows.h>

#include "content/browser/debug/debug_agent.h"
#include "content/public/map_types.h"
#include "content/public/view_host.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/map/viewport/draw_host.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "base/process/switches.h"

namespace app {
namespace {

void console_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  if (!detail::exe_capture_path(path, MAX_PATH, L"self-test-mark.txt")) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
  }
}

void console_detach_maps(Browser& browser) {
  detail::detach_maps(browser);
}

const ui::views::View* find_view_by_paint_role(const ui::views::View* root,
                                               std::string_view name) {
  if (!root) {
    return nullptr;
  }
  if (root->paint_role() == name) {
    return root;
  }
  for (size_t i = 0; i < root->child_count(); ++i) {
    if (const ui::views::View* hit =
            find_view_by_paint_role(root->child_at(i), name)) {
      return hit;
    }
  }
  return nullptr;
}

void append_view_tree_lines(const ui::views::View* view, int depth,
                            std::ostringstream& oss) {
  if (!view) {
    return;
  }
  for (int i = 0; i < depth; ++i) {
    oss << ' ';
  }
  const std::string_view role = view->paint_role();
  const ui::views::Rect b = view->bounds();
  oss << (role.empty() ? "view" : role) << " [" << b.x << ',' << b.y << ' '
      << b.width << 'x' << b.height << "]\n";
  for (size_t i = 0; i < view->child_count(); ++i) {
    append_view_tree_lines(view->child_at(i), depth + 1, oss);
  }
}

// Mirror BrowserView::toggle_debug_console host hooks (refresh/extent/layers/
// ui_*), using Browser APIs. ui_click/ui_type are best-effort stubs without
// Widget access from this TU.
void wire_debug_agent_host(Browser& browser) {
  content::DebugAgentHost host;
  host.refresh_map = [&browser] {
    browser.on_view_command("view.refresh", -1, false, 0, 0);
  };
  host.extent_string = [&browser] {
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    if (!browser.document()->compute_extent(&min_x, &min_y, &max_x, &max_y)) {
      return std::string("empty extent");
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.6f,%.6f,%.6f,%.6f", min_x, min_y, max_x,
                  max_y);
    return std::string(buf);
  };
  host.layer_names = [&browser] {
    std::vector<std::string> names;
    if (!browser.document()) {
      return names;
    }
    names.push_back("features=" +
                    std::to_string(browser.document()->feature_count()));
    for (const auto& d : browser.document()->layer_descs()) {
      names.push_back(d.name);
    }
    return names;
  };
  host.ui_find = [&browser](const std::string& name) {
    ui::views::View* root = browser.contents_view();
    if (!root) {
      return std::string("not wired");
    }
    const ui::views::View* hit =
        find_view_by_paint_role(root, std::string_view(name));
    if (!hit) {
      return std::string("not wired");
    }
    const ui::views::Rect b = hit->bounds();
    std::ostringstream oss;
    oss << "{\"role\":\"" << hit->paint_role() << "\",\"x\":" << b.x
        << ",\"y\":" << b.y << ",\"w\":" << b.width << ",\"h\":" << b.height
        << "}";
    return oss.str();
  };
  host.ui_click = [](int, int, int) { return std::string("clicked"); };
  host.ui_type = [](const std::string& utf8) {
    return std::string("typed ") + std::to_string(utf8.size()) + " chars";
  };
  host.ui_dump_tree = [&browser]() {
    ui::views::View* root = browser.contents_view();
    if (!root) {
      return std::string("not wired");
    }
    std::ostringstream oss;
    append_view_tree_lines(root, 0, oss);
    return oss.str();
  };
  host.ui_overlay_stats = []() { return std::string("unavailable"); };
  content::debug_agent().set_host(std::move(host));
}

bool try_open_china_sample(Browser& browser) {
  if (browser.document() && browser.document()->layer_count() > 0 &&
      browser.document()->feature_count() >= 3) {
    return true;
  }
  wchar_t sample_w[MAX_PATH] = {};
  if (!detail::exe_dir_with_slash(sample_w, MAX_PATH)) {
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
  // Real-data policy: never write a GeoJSON stub on miss.
  return false;
}

double elapsed_ms(LARGE_INTEGER freq, LARGE_INTEGER t0, LARGE_INTEGER t1) {
  return (static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0) /
         static_cast<double>(freq.QuadPart);
}

double median_ms(std::vector<double>* samples) {
  if (!samples || samples->empty()) {
    return 0.0;
  }
  std::sort(samples->begin(), samples->end());
  const size_t n = samples->size();
  if (n % 2 == 1) {
    return (*samples)[n / 2];
  }
  return 0.5 * ((*samples)[n / 2 - 1] + (*samples)[n / 2]);
}

bool write_console_bench_json(double layers_list_ms, double extent_ms,
                              double refresh_ms, double pan_frame_p50_ms,
                              bool dem_ok, bool tiles_ok) {
  char path_a[MAX_PATH] = {};
  const char* env = base::switch_cstr("console-bench-json");
  if (env && env[0]) {
    if (strcpy_s(path_a, env) != 0) {
      return false;
    }
  } else if (!detail::exe_capture_path_a(path_a, MAX_PATH,
                                         "console_bench.json")) {
    return false;
  }
  FILE* f = nullptr;
  if (fopen_s(&f, path_a, "wb") != 0 || !f) {
    return false;
  }
  std::fprintf(f,
               "{\"layers_list_ms\":%.3f,\"extent_ms\":%.3f,"
               "\"refresh_ms\":%.3f,\"pan_frame_p50_ms\":%.3f,"
               "\"dem_ok\":%s,\"tiles_ok\":%s}\n",
               layers_list_ms, extent_ms, refresh_ms, pan_frame_p50_ms,
               dem_ok ? "true" : "false", tiles_ok ? "true" : "false");
  std::fclose(f);
  return true;
}

}  // namespace

int console_self_test_body(Browser& browser) {
  wchar_t mark_path[MAX_PATH] = {};
  if (detail::exe_capture_path(mark_path, MAX_PATH, L"self-test-mark.txt")) {
    DeleteFileW(mark_path);
  }
  console_mark("show");
  pump_views_messages(400);
  if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
    std::fprintf(stderr, "console self-test: no hwnd\n");
    return 2;
  }
  console_mark("hwnd-ok");

  ui::views::DrawHost* map = browser.draw_host();
  if (map &&
      map->attach_mode() ==
          ui::views::DrawHost::AttachMode::kContentMapView) {
    if (!map->wait_ready(20000)) {
      std::fprintf(stderr, "console self-test: map wait_ready failed\n");
      console_detach_maps(browser);
      return 3;
    }
  }
  console_mark("map-ready");

  // KillTimer alone leaves queued WM_TIMER; pause_present drains them.
  if (map) {
    map->pause_present();
  }
  if (ui::views::DrawHost* data = browser.data_draw_host()) {
    data->pause_present();
  }
  if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
    scene->pause_present();
  }

  wire_debug_agent_host(browser);
  if (!content::debug_agent().is_running()) {
    if (!content::debug_agent().start()) {
      std::fprintf(stderr, "console self-test: debug_agent start failed\n");
      console_detach_maps(browser);
      return 50;
    }
  }

  if (!try_open_china_sample(browser)) {
    std::fprintf(stderr, "console self-test: china sample open failed\n");
    console_detach_maps(browser);
    return 26;
  }
  browser.fit_map_extent();
  pump_views_messages(100);

  const std::string help = content::debug_agent().exec_line(":help");
  if (help.find(":layers") == std::string::npos) {
    std::fprintf(stderr, "console self-test: :help failed\n");
    console_detach_maps(browser);
    return 51;
  }

  LARGE_INTEGER freq{};
  LARGE_INTEGER t0{};
  LARGE_INTEGER t1{};
  QueryPerformanceFrequency(&freq);

  QueryPerformanceCounter(&t0);
  const std::string layers = content::debug_agent().exec_line(":layers");
  QueryPerformanceCounter(&t1);
  const double layers_list_ms = elapsed_ms(freq, t0, t1);
  if (layers.empty() || layers.find("no host.layer_names") != std::string::npos) {
    std::fprintf(stderr, "console self-test: :layers failed\n");
    console_detach_maps(browser);
    return 52;
  }

  QueryPerformanceCounter(&t0);
  const std::string extent = content::debug_agent().exec_line(":extent");
  QueryPerformanceCounter(&t1);
  const double extent_ms = elapsed_ms(freq, t0, t1);
  if (extent.find("no host.extent_string") != std::string::npos ||
      extent.find("empty") != std::string::npos) {
    std::fprintf(stderr, "console self-test: :extent failed\n");
    console_detach_maps(browser);
    return 53;
  }

  QueryPerformanceCounter(&t0);
  const std::string refresh = content::debug_agent().exec_line(":refresh");
  QueryPerformanceCounter(&t1);
  const double refresh_ms = elapsed_ms(freq, t0, t1);
  if (refresh != "refreshed") {
    std::fprintf(stderr, "console self-test: :refresh failed (%s)\n",
                 refresh.c_str());
    console_detach_maps(browser);
    return 54;
  }
  console_mark("console-ok");

  // Viewport MapLibre-style: activate view.pan and time a few pan gestures.
  std::vector<double> pan_samples;
  if (!browser.run_tool_command("view.pan")) {
    std::fprintf(stderr, "console self-test: view.pan failed\n");
    console_detach_maps(browser);
    return 40;
  }
  content::ViewHost* host = browser.edit_view_host();
  tool::Interaction* cur =
      host && host->workspace() ? host->workspace()->stack().current()
                                : nullptr;
  if (!cur || std::strcmp(cur->id(), "view.pan") != 0) {
    std::fprintf(stderr, "console self-test: view.pan not active\n");
    console_detach_maps(browser);
    return 41;
  }
  for (int i = 0; i < 5; ++i) {
    const int x0 = 40 + i * 4;
    const int y0 = 40 + i * 2;
    const int x1 = x0 + 30;
    const int y1 = y0 + 15;
    content::InputEvent pan_down{};
    pan_down.kind = content::InputEvent::Kind::kLDown;
    pan_down.x_px = x0;
    pan_down.y_px = y0;
    content::InputEvent pan_move{};
    pan_move.kind = content::InputEvent::Kind::kMouseMove;
    pan_move.x_px = x1;
    pan_move.y_px = y1;
    content::InputEvent pan_up{};
    pan_up.kind = content::InputEvent::Kind::kLUp;
    pan_up.x_px = x1;
    pan_up.y_px = y1;
    QueryPerformanceCounter(&t0);
    if (!host->dispatch_input(pan_down) || !host->dispatch_input(pan_move) ||
        !host->dispatch_input(pan_up)) {
      std::fprintf(stderr, "console self-test: pan dispatch failed\n");
      console_detach_maps(browser);
      return 42;
    }
    QueryPerformanceCounter(&t1);
    pan_samples.push_back(elapsed_ms(freq, t0, t1));
  }
  // One wheel zoom roundtrip (best-effort timing sample).
  if (host) {
    content::InputEvent wheel{};
    wheel.kind = content::InputEvent::Kind::kWheel;
    wheel.x_px = 40;
    wheel.y_px = 40;
    wheel.wheel = -120;
    QueryPerformanceCounter(&t0);
    (void)host->dispatch_input(wheel);
    QueryPerformanceCounter(&t1);
    pan_samples.push_back(elapsed_ms(freq, t0, t1));
  }
  const double pan_frame_p50_ms = median_ms(&pan_samples);

  // Soft DEM/tile marks: record when already available; never hard-fail.
  bool dem_ok = false;
  bool tiles_ok = false;
  if (browser.map2d() && browser.map2d()->basemap_tiles_drawn() > 0) {
    tiles_ok = true;
    console_mark("tiles-ok");
  }
  // Short console path does not seed DEM; leave dem_ok false when absent.

  if (!write_console_bench_json(layers_list_ms, extent_ms, refresh_ms,
                                pan_frame_p50_ms, dem_ok, tiles_ok)) {
    std::fprintf(stderr, "console self-test: console_bench.json write failed\n");
    console_detach_maps(browser);
    return 55;
  }
  console_mark("console-bench-ok");

  console_detach_maps(browser);
  console_mark("detached");
  return 0;
}

int run_views_console_self_test(Browser& browser) {
  if (try_run_suite_script(browser, "console", detail::kSelfTestMarkLeaf,
                           /*clear_marks=*/true)) {
    return 0;
  }
  return console_self_test_body(browser);
}

}  // namespace app
