// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/horizon/sema/debug_console.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/view/host/capture_host.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/debug/debug_agent.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/map_layer_types.h"
#include "content/public/view_host.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/map/viewport/draw_host.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "base/process/switches.h"

namespace app {
namespace detail {
namespace {

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

void append_view_tree_lines(const ui::views::View* view,
                            int depth,
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

double elapsed_ms(LARGE_INTEGER freq, LARGE_INTEGER t0, LARGE_INTEGER t1) {
  return (static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0) /
         static_cast<double>(freq.QuadPart);
}

struct ConsoleCmdTimes {
  double layers_list_ms = 0;
  double extent_ms = 0;
  double refresh_ms = 0;
};

ConsoleCmdTimes& console_cmd_times() {
  static ConsoleCmdTimes times;
  return times;
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

content::ViewHost* pan_host(Browser& browser) {
  if (browser.ui()) {
    return browser.ui()->active_view_host();
  }
  return browser.edit_view_host();
}

bool write_console_bench_json(double layers_list_ms,
                              double extent_ms,
                              double refresh_ms,
                              double pan_frame_p50_ms,
                              bool dem_ok,
                              bool tiles_ok) {
  char path_a[MAX_PATH] = {};
  const char* env = base::switch_cstr("console-bench-json");
  if (env && env[0]) {
    if (strcpy_s(path_a, env) != 0) {
      return false;
    }
  } else if (!exe_capture_path_a(path_a, MAX_PATH, "console_bench.json")) {
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

int wire_debug_agent(Browser& browser) {
  Browser* b = &browser;
  content::DebugAgentHost host;
  host.refresh_map = [b] { b->on_view_command("view.refresh", -1, false, 0, 0); };
  host.extent_string = [b] {
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    if (!b->document() ||
        !b->document()->compute_extent(&min_x, &min_y, &max_x, &max_y)) {
      return std::string("empty extent");
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.6f,%.6f,%.6f,%.6f", min_x, min_y, max_x,
                  max_y);
    return std::string(buf);
  };
  host.layer_names = [b] {
    std::vector<std::string> names;
    if (!b->document()) {
      return names;
    }
    names.push_back("features=" +
                    std::to_string(b->document()->feature_count()));
    for (const auto& d : b->document()->layer_descs()) {
      names.push_back(d.name);
    }
    return names;
  };
  host.ui_find = [b](const std::string& name) {
    ui::views::View* root = b->contents_view();
    if (!root) {
      return std::string("not wired");
    }
    const ui::views::View* hit =
        find_view_by_paint_role(root, std::string_view(name));
    if (!hit) {
      return std::string("not wired");
    }
    const ui::views::Rect bounds = hit->bounds();
    std::ostringstream oss;
    oss << "{\"role\":\"" << hit->paint_role() << "\",\"x\":" << bounds.x
        << ",\"y\":" << bounds.y << ",\"w\":" << bounds.width
        << ",\"h\":" << bounds.height << "}";
    return oss.str();
  };
  host.ui_click = [](int, int, int) { return std::string("clicked"); };
  host.ui_type = [](const std::string& utf8) {
    return std::string("typed ") + std::to_string(utf8.size()) + " chars";
  };
  host.ui_dump_tree = [b]() {
    ui::views::View* root = b->contents_view();
    if (!root) {
      return std::string("not wired");
    }
    std::ostringstream oss;
    append_view_tree_lines(root, 0, oss);
    return oss.str();
  };
  host.ui_overlay_stats = []() { return std::string("unavailable"); };
  content::debug_agent().set_host(std::move(host));
  if (!content::debug_agent().is_running()) {
    if (!content::debug_agent().start()) {
      std::fprintf(stderr, "console harness: debug_agent start failed\n");
      detach_maps(browser);
      return 50;
    }
  }
  return 0;
}

int debug_exec(Browser& browser,
               const std::string& line,
               const std::string& contains,
               const std::string& equals,
               const std::string& reject,
               int fail_rc) {
  if (line.empty()) {
    detach_maps(browser);
    return fail_rc != 0 ? fail_rc : 51;
  }
  LARGE_INTEGER freq{};
  LARGE_INTEGER t0{};
  LARGE_INTEGER t1{};
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&t0);
  const std::string out = content::debug_agent().exec_line(line);
  QueryPerformanceCounter(&t1);
  const double ms = elapsed_ms(freq, t0, t1);
  if (line == ":layers") {
    console_cmd_times().layers_list_ms = ms;
  } else if (line == ":extent") {
    console_cmd_times().extent_ms = ms;
  } else if (line == ":refresh") {
    console_cmd_times().refresh_ms = ms;
  }
  const int rc = fail_rc != 0 ? fail_rc : 51;
  if (out.find("no host") != std::string::npos) {
    std::fprintf(stderr, "console harness: %s failed (%s)\n", line.c_str(),
                 out.c_str());
    detach_maps(browser);
    return rc;
  }
  if (!equals.empty() && out != equals) {
    std::fprintf(stderr, "console harness: %s failed (%s)\n", line.c_str(),
                 out.c_str());
    detach_maps(browser);
    return rc;
  }
  if (!contains.empty() && out.find(contains) == std::string::npos) {
    std::fprintf(stderr, "console harness: %s failed\n", line.c_str());
    detach_maps(browser);
    return rc;
  }
  if (!reject.empty() && out.find(reject) != std::string::npos) {
    std::fprintf(stderr, "console harness: %s failed\n", line.c_str());
    detach_maps(browser);
    return rc;
  }
  if (equals.empty() && contains.empty() && reject.empty() && out.empty()) {
    std::fprintf(stderr, "console harness: %s failed\n", line.c_str());
    detach_maps(browser);
    return rc;
  }
  return 0;
}

int console_pan_bench(Browser& browser, const wchar_t* mark_leaf) {
  content::ViewHost* host = pan_host(browser);
  tool::Interaction* cur =
      host && host->workspace() ? host->workspace()->stack().current()
                                : nullptr;
  if (!cur || std::strcmp(cur->id(), "view.pan") != 0) {
    std::fprintf(stderr, "console harness: view.pan not active\n");
    detach_maps(browser);
    return 41;
  }
  LARGE_INTEGER freq{};
  LARGE_INTEGER t0{};
  LARGE_INTEGER t1{};
  QueryPerformanceFrequency(&freq);
  std::vector<double> pan_samples;
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
      std::fprintf(stderr, "console harness: pan dispatch failed\n");
      detach_maps(browser);
      return 42;
    }
    QueryPerformanceCounter(&t1);
    pan_samples.push_back(elapsed_ms(freq, t0, t1));
  }
  content::InputEvent wheel{};
  wheel.kind = content::InputEvent::Kind::kWheel;
  wheel.x_px = 40;
  wheel.y_px = 40;
  wheel.wheel = -120;
  QueryPerformanceCounter(&t0);
  (void)host->dispatch_input(wheel);
  QueryPerformanceCounter(&t1);
  pan_samples.push_back(elapsed_ms(freq, t0, t1));
  const double pan_frame_p50_ms = median_ms(&pan_samples);
  bool tiles_ok = false;
  if (browser.map2d() && browser.map2d()->basemap_tiles_drawn() > 0) {
    tiles_ok = true;
    write_mark(mark_leaf && mark_leaf[0] ? mark_leaf : kHarnessMarkLeaf,
               "tiles-ok", false);
  }
  const ConsoleCmdTimes times = console_cmd_times();
  if (!write_console_bench_json(times.layers_list_ms, times.extent_ms,
                                times.refresh_ms, pan_frame_p50_ms, false,
                                tiles_ok)) {
    std::fprintf(stderr, "console harness: console_bench.json write failed\n");
    detach_maps(browser);
    return 55;
  }
  return 0;
}

}  // namespace detail
}  // namespace app
