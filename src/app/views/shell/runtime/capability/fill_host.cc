// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/capability/fill_host.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "app/views/shell/app/cmdline/views_launch_options.h"
#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/common/pump.h"
#include "app/views/shell/harness/common/sample.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/atmosphere/atmosphere_showcase.h"
#include "app/views/shell/harness/showcase/map2d/map2d_showcase.h"
#include "app/views/shell/harness/showcase/plugin/plugin_showcase.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "gis/model/edit/session/memory_edit_session.h"
#include "gis/present/style/style_document.h"
#include "render/rhi/rhi.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/collection/tab_strip.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace {

constexpr int kExportW = 640;
constexpr int kExportH = 480;

bool wide_to_utf8(const wchar_t* wide, char* out, size_t out_cap) {
  if (!wide || !out || out_cap < 2) {
    return false;
  }
  return WideCharToMultiByte(CP_UTF8, 0, wide, -1, out,
                             static_cast<int>(out_cap), nullptr, nullptr) > 0;
}

bool resolve_rel_under_exe(const wchar_t* const* rels,
                           size_t count,
                           std::string* out_utf8) {
  if (!out_utf8 || !rels || count == 0) {
    return false;
  }
  wchar_t base[MAX_PATH] = {};
  if (!detail::exe_dir_with_slash(base, MAX_PATH)) {
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
    wchar_t canon[MAX_PATH] = {};
    const wchar_t* use = full;
    if (GetFullPathNameW(full, MAX_PATH, canon, nullptr) != 0) {
      use = canon;
    }
    char utf8[MAX_PATH * 3] = {};
    if (!wide_to_utf8(use, utf8, sizeof(utf8))) {
      continue;
    }
    *out_utf8 = utf8;
    return true;
  }
  return false;
}

bool find_named_under(const std::wstring& root,
                      const std::wstring& leaf,
                      std::wstring* out,
                      int depth) {
  if (!out || depth > 8) {
    return false;
  }
  const std::wstring pattern = root + L"\\*";
  WIN32_FIND_DATAW fd = {};
  HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) {
    return false;
  }
  bool found = false;
  do {
    if (fd.cFileName[0] == L'.' &&
        (fd.cFileName[1] == L'\0' ||
         (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) {
      continue;
    }
    const std::wstring child = root + L"\\" + fd.cFileName;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (find_named_under(child, leaf, out, depth + 1)) {
        found = true;
        break;
      }
      continue;
    }
    if (_wcsicmp(fd.cFileName, leaf.c_str()) == 0 &&
        GetFileAttributesW(child.c_str()) != INVALID_FILE_ATTRIBUTES) {
      *out = child;
      found = true;
      break;
    }
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  return found;
}

bool resolve_harness_leaf(const std::string& leaf_utf8, std::string* out_utf8) {
  if (!out_utf8 || leaf_utf8.empty()) {
    return false;
  }
  // Non-recursive: only well-known shallow layouts under harness/.
  wchar_t exe[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) {
    return false;
  }
  std::wstring dir(exe);
  const size_t slash = dir.find_last_of(L"\\/");
  if (slash == std::wstring::npos) {
    return false;
  }
  dir.resize(slash);
  const int n = MultiByteToWideChar(CP_UTF8, 0, leaf_utf8.c_str(), -1, nullptr,
                                    0);
  if (n <= 1) {
    return false;
  }
  std::wstring leaf_w(static_cast<size_t>(n - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, leaf_utf8.c_str(), -1, leaf_w.data(), n);
  const std::wstring harness_rels[] = {
      dir + L"\\..\\..\\testing\\tools\\harness",
      dir + L"\\..\\..\\..\\testing\\tools\\harness",
  };
  // suite dirs: plugin/<suite_id>/leaf, _shared/scripts/interact/leaf, shell/...
  const wchar_t* subdirs[] = {
      L"plugin\\plugin.world3d",
      L"plugin\\plugin.orthogrid",
      L"plugin\\plugin.orthogrid3d",
      L"plugin\\plugin.traffic",
      L"plugin\\plugin.flood",
      L"plugin\\plugin.stormsurge",
      L"plugin\\plugin.mine",
      L"plugin\\plugin.print",
      L"_shared\\scripts\\interact",
  };
  for (const std::wstring& harness : harness_rels) {
    wchar_t abs_buf[MAX_PATH] = {};
    const DWORD got =
        GetFullPathNameW(harness.c_str(), MAX_PATH, abs_buf, nullptr);
    if (got == 0 || got >= MAX_PATH) {
      continue;
    }
    for (const wchar_t* sub : subdirs) {
      wchar_t cand[MAX_PATH] = {};
      if (swprintf_s(cand, MAX_PATH, L"%s\\%s\\%s", abs_buf, sub,
                     leaf_w.c_str()) <= 0) {
        continue;
      }
      if (GetFileAttributesW(cand) == INVALID_FILE_ATTRIBUTES) {
        continue;
      }
      char utf8[MAX_PATH * 3] = {};
      if (!wide_to_utf8(cand, utf8, sizeof(utf8))) {
        continue;
      }
      *out_utf8 = utf8;
      return true;
    }
  }
  return false;
}

bool frame_for_export(Browser& browser, const std::string& frame) {
  content::ViewFrame* vf = browser.view_frame();
  if (!vf) {
    return false;
  }
  if (frame == "china_product") {
    ensure_china_maplibre_carto(browser);
    frame_china_map2d(browser, kExportW, kExportH);
  } else if (frame == "unit_square") {
    constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
    vf->apply_world_extent(kUnit, kExportW, kExportH);
  } else if (frame == "traffic_beijing") {
    constexpr content::Extent2 kBeijing{116.34, 39.885, 116.46, 39.930};
    vf->apply_world_extent(kBeijing, kExportW, kExportH);
  } else if (frame == "flood_wuhan") {
    constexpr content::Extent2 kWuhan{114.15, 30.45, 114.45, 30.65};
    vf->apply_world_extent(kWuhan, kExportW, kExportH);
  } else if (frame == "mine_boreholes") {
    constexpr content::Extent2 kMine{116.34, 39.87, 116.41, 39.93};
    vf->apply_world_extent(kMine, kExportW, kExportH);
  } else if (frame == "stormsurge_coast") {
    constexpr content::Extent2 kCoast{114.15, 30.45, 114.45, 30.65};
    vf->apply_world_extent(kCoast, kExportW, kExportH);
  } else if (frame == "geochem_samples") {
    // Matches testing/data/plugin/geochem_samples.csv Wuhan-window cluster.
    constexpr content::Extent2 kGeochem{114.18, 30.45, 114.36, 30.60};
    vf->apply_world_extent(kGeochem, kExportW, kExportH);
  } else if (frame == "world3d_tin") {
    constexpr content::Extent2 kTin{104.7, 34.7, 105.8, 35.8};
    vf->apply_world_extent(kTin, kExportW, kExportH);
  } else if (frame == "document_extent") {
    double minx = 0.0;
    double miny = 0.0;
    double maxx = 0.0;
    double maxy = 0.0;
    if (browser.document() &&
        browser.document()->compute_extent(&minx, &miny, &maxx, &maxy) &&
        maxx > minx && maxy > miny) {
      // compute_extent returns map space (y = -lat). apply_world_extent expects
      // lon/lat Extent2 and converts to map internally — convert here once.
      const double lat_min = -maxy;
      const double lat_max = -miny;
      const double pad_x = std::max(0.05, (maxx - minx) * 0.15);
      const double pad_y = std::max(0.05, (lat_max - lat_min) * 0.15);
      const content::Extent2 live{minx - pad_x, lat_min - pad_y, maxx + pad_x,
                                  lat_max + pad_y};
      vf->apply_world_extent(live, kExportW, kExportH);
    } else {
      constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
      vf->apply_world_extent(kUnit, kExportW, kExportH);
    }
  } else {
    return false;
  }
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  return true;
}

}  // namespace

void fill_host(Browser& browser,
                          content::CapabilityHost* out,
                          const wchar_t* mark_leaf) {
  if (!out) {
    return;
  }
  Browser* b = &browser;
  const wchar_t* leaf =
      mark_leaf && mark_leaf[0] ? mark_leaf : detail::kUiShowcaseMarkLeaf;

  out->pump = [](int ms) {
    detail::pump_messages(static_cast<DWORD>(ms > 0 ? ms : 0));
  };
  out->mark = [leaf](const std::string& token) {
    detail::write_mark(leaf, token.c_str(), false);
  };
  out->select_map_tab = [b](int index) { b->select_map_tab(index); };
  out->catalog_tab = [b](int index) {
    if (ui::views::CatalogView* cat = b->catalog_view()) {
      if (ui::views::TabStrip* tabs = cat->source_tabs()) {
        tabs->set_active(index);
      }
    }
  };
  out->inspector_tab = [b](int index) {
    if (b->ui()) {
      if (ui::views::TabStrip* insp = b->ui()->inspector_tabs()) {
        if (index >= 0 && index < insp->tab_count()) {
          insp->set_active(index);
        }
      }
    }
  };
  out->shell_hwnd = [b]() -> void* {
    return reinterpret_cast<void*>(b->hwnd());
  };
  out->dispatch_edit_input = [b](const content::InputEvent& e) {
    content::ViewHost* host = b->edit_view_host();
    return host && host->dispatch_input(e);
  };
  out->wait_map_ready = [b](int timeout_ms) {
    ui::views::MapViewport* map = b->map_viewport();
    if (!map) {
      return false;
    }
    return map->wait_ready(timeout_ms > 0 ? timeout_ms : 5000);
  };
  out->load_china_sample = [b](bool write_stub) {
    return detail::try_open_china_sample(*b, write_stub);
  };
  out->detach_maps = [b]() { detail::detach_maps(*b); };
  out->stop_map_present_timers = [b]() {
    detail::stop_map_present_timers(*b);
  };
  out->window = [b](const std::string& action, int w, int h) {
    HWND hwnd = b->hwnd();
    if (!hwnd || !IsWindow(hwnd)) {
      return false;
    }
    if (action == "activate") {
      ShowWindow(hwnd, SW_SHOW);
      SetForegroundWindow(hwnd);
      return true;
    }
    if (action == "resize") {
      MoveWindow(hwnd, 0, 0, w > 0 ? w : 1280, h > 0 ? h : 800, TRUE);
      return true;
    }
    return true;
  };
  out->key = [b](unsigned vk) {
    HWND hwnd = b->hwnd();
    if (!vk || !hwnd || !IsWindow(hwnd)) {
      return false;
    }
    PostMessageW(hwnd, WM_KEYDOWN, vk, 0);
    PostMessageW(hwnd, WM_KEYUP, vk, 0);
    return true;
  };
  out->clear_marks = [leaf]() { detail::clear_mark(leaf); };
  out->edit_host_ready = [b]() {
    content::ViewHost* host = b->edit_view_host();
    if (!host || !host->workspace() || !host->edits()) {
      return false;
    }
    return dynamic_cast<gis::MemoryEditSession*>(host->edits()) != nullptr;
  };
  out->run_tool = [b](const std::string& command_id) {
    return b->run_tool_command(command_id);
  };
  out->current_tool_id = [b]() -> std::string {
    // Match run_tool_command: Scene3D / Data tabs own their ViewHost stacks.
    content::ViewHost* host = nullptr;
    if (ui::views::MapViewport* scene = b->map_scene_viewport()) {
      if (scene->is_visible()) {
        host = b->scene_host();
      }
    }
    if (!host) {
      if (ui::views::MapViewport* data = b->map_data_viewport()) {
        if (data->is_visible()) {
          host = b->data_host();
        }
      }
    }
    if (!host) {
      host = b->edit_view_host();
    }
    if (!host || !host->workspace()) {
      return {};
    }
    tool::Interaction* cur = host->workspace()->stack().current();
    if (!cur || !cur->id()) {
      return {};
    }
    return std::string(cur->id());
  };
  out->expect_last_geom = [b](const std::string& kind, int min_points) {
    content::ViewHost* host = b->edit_view_host();
    if (!host || !host->edits()) {
      return false;
    }
    auto* mem = dynamic_cast<gis::MemoryEditSession*>(host->edits());
    if (!mem || mem->committed_count() < 1) {
      return false;
    }
    gis::FeatureGeom::Kind want = gis::FeatureGeom::Kind::kNone;
    if (kind == "point") {
      want = gis::FeatureGeom::Kind::kPoint;
    } else if (kind == "linestring" || kind == "line") {
      want = gis::FeatureGeom::Kind::kLineString;
    } else if (kind == "polygon" || kind == "poly") {
      want = gis::FeatureGeom::Kind::kPolygon;
    } else {
      return false;
    }
    const gis::FeatureMutation got =
        mem->committed_at(mem->committed_count() - 1);
    const int need = min_points > 0 ? min_points : 1;
    return got.op == gis::EditOp::kAppend && !got.geom.empty() &&
           got.geom.kind == want &&
           static_cast<int>(got.geom.points.size()) >= need;
  };
  out->browse_stress = [b, leaf](int count) {
    content::ViewHost* host = b->edit_view_host();
    if (!host) {
      detail::write_mark(leaf, "browse-stress-no-host", false);
      return false;
    }
    detail::write_mark(leaf, "browse-stress-begin", false);
    detail::stop_map_present_timers(*b);
    // No UI pump here or in the burst — PeekMessage+Dispatch re-enters paint
    // while the next dispatch_input mutates camera and AVs (marks stop at
    // browse-stress-begin). Sleep settles in-flight present without re-entry.
    ::Sleep(50);
    detail::stop_map_present_timers(*b);
    SetEnvironmentVariableA("SMT_SKIP_MAP_CONTEXT_MENU", "1");
    const int n = count > 0 ? count : 24;
    for (int i = 0; i < n; ++i) {
      if ((i % 8) == 0) {
        detail::stop_map_present_timers(*b);
        char step[32];
        std::snprintf(step, sizeof(step), "browse-stress-%d", i);
        detail::write_mark(leaf, step, false);
      }
      content::InputEvent pan_down{};
      pan_down.kind = content::InputEvent::Kind::kLDown;
      pan_down.x_px = 30 + (i % 5) * 8;
      pan_down.y_px = 30 + (i % 7) * 6;
      content::InputEvent pan_move = pan_down;
      pan_move.kind = content::InputEvent::Kind::kMouseMove;
      pan_move.x_px += 18;
      pan_move.y_px += 12;
      content::InputEvent pan_up = pan_move;
      pan_up.kind = content::InputEvent::Kind::kLUp;
      if (!host->dispatch_input(pan_down) || !host->dispatch_input(pan_move) ||
          !host->dispatch_input(pan_up)) {
        SetEnvironmentVariableA("SMT_SKIP_MAP_CONTEXT_MENU", nullptr);
        detail::write_mark(leaf, "browse-stress-pan-fail", false);
        return false;
      }
      content::InputEvent wheel{};
      wheel.kind = content::InputEvent::Kind::kWheel;
      wheel.x_px = pan_move.x_px;
      wheel.y_px = pan_move.y_px;
      wheel.wheel = (i & 1) ? 120 : -120;
      if (!host->dispatch_input(wheel)) {
        SetEnvironmentVariableA("SMT_SKIP_MAP_CONTEXT_MENU", nullptr);
        detail::write_mark(leaf, "browse-stress-wheel-fail", false);
        return false;
      }
      ::Sleep(20);
    }
    ::Sleep(50);
    content::InputEvent rdown{};
    rdown.kind = content::InputEvent::Kind::kRDown;
    rdown.x_px = 50;
    rdown.y_px = 50;
    content::InputEvent rup = rdown;
    rup.kind = content::InputEvent::Kind::kRUp;
    // view.pan must not swallow RMB (shell owns the context menu).
    if (host->dispatch_input(rdown) || host->dispatch_input(rup)) {
      SetEnvironmentVariableA("SMT_SKIP_MAP_CONTEXT_MENU", nullptr);
      detail::write_mark(leaf, "browse-stress-rmb-swallowed", false);
      return false;
    }
    SetEnvironmentVariableA("SMT_SKIP_MAP_CONTEXT_MENU", nullptr);
    detail::write_mark(leaf, "browse-stress-end", false);
    return true;
  };
  out->expect_wheel_cursor = [b, leaf](int x, int y) {
    content::ViewHost* host = b->edit_view_host();
    if (!host || !b->view_frame() || !b->orbit_frame()) {
      detail::write_mark(leaf, "wheel-cursor-no-frame", false);
      return false;
    }
    const double scale0 = b->view_frame()->scale();
    content::InputEvent wheel{};
    wheel.kind = content::InputEvent::Kind::kWheel;
    wheel.x_px = x;
    wheel.y_px = y;
    wheel.wheel = -120;
    if (!host->dispatch_input(wheel)) {
      return false;
    }
    if (std::fabs(b->view_frame()->scale() - scale0) < 1e-9) {
      wheel.wheel = 120;
      if (!host->dispatch_input(wheel) ||
          std::fabs(b->view_frame()->scale() - scale0) < 1e-9) {
        return false;
      }
    }
    const render::rhi::CameraMatrices ortho =
        b->orbit_frame()->camera_matrices_ortho(800.f, 600.f);
    return ortho.kind == render::rhi::CameraKind::kOrtho;
  };
  out->map2d_run = [b](const std::string& mode) {
    Map2dShowcaseMode m = Map2dShowcaseMode::kNone;
    if (mode == "china") {
      m = Map2dShowcaseMode::kChina;
    } else if (mode == "align") {
      m = Map2dShowcaseMode::kAlign;
    } else if (mode == "orthogrid") {
      m = Map2dShowcaseMode::kOrthogrid;
    } else {
      return false;
    }
    return map2d_showcase_body(*b, m) == 0;
  };
  out->atmosphere_run = [b](const std::string& mode) {
    AtmosphereShowcaseMode m = AtmosphereShowcaseMode::kNone;
    if (mode == "land") {
      m = AtmosphereShowcaseMode::kLand;
    } else if (mode == "ocean") {
      m = AtmosphereShowcaseMode::kOcean;
    } else if (mode == "full") {
      m = AtmosphereShowcaseMode::kFull;
    } else if (mode == "coast") {
      m = AtmosphereShowcaseMode::kCoast;
    } else if (mode == "globe" || mode == "earth") {
      m = AtmosphereShowcaseMode::kGlobe;
    } else {
      return false;
    }
    return atmosphere_showcase_body(*b, m) == 0;
  };
  out->plugin_run = [b](const std::string& mode) {
    PluginShowcaseMode m = PluginShowcaseMode::kNone;
    if (mode == "world3d" || mode == "dem") {
      m = PluginShowcaseMode::kWorld3d;
    } else if (mode == "print") {
      m = PluginShowcaseMode::kPrint;
    } else if (mode == "orthogrid" || mode == "baogrid") {
      m = PluginShowcaseMode::kOrthogrid;
    } else if (mode == "orthogrid3d" || mode == "hexgrid") {
      m = PluginShowcaseMode::kOrthogrid3d;
    } else if (mode == "traffic") {
      m = PluginShowcaseMode::kTraffic;
    } else if (mode == "flood") {
      m = PluginShowcaseMode::kFlood;
    } else if (mode == "stormsurge") {
      m = PluginShowcaseMode::kStormSurge;
    } else if (mode == "mine") {
      m = PluginShowcaseMode::kMine;
    } else if (mode == "geochem") {
      m = PluginShowcaseMode::kGeochem;
    } else {
      return false;
    }
    return plugin_showcase_body(*b, m) == 0;
  };
  out->run_processing = [b](const std::string& id, const std::string& args) {
    PluginShell* shell = b->plugins();
    if (!shell || id.empty() || !shell->host()) {
      return false;
    }
    return shell->run_processing(id, args);
  };
  out->console_run = [b]() { return console_self_test_body(*b) == 0; };

  out->resolve_data = [](const std::string& kind, const std::string& leaf,
                         std::string* out_path) {
    if (!out_path || leaf.empty()) {
      return false;
    }
    if (kind == "harness") {
      return resolve_harness_leaf(leaf, out_path);
    }
    wchar_t a[MAX_PATH] = {};
    wchar_t bpath[MAX_PATH] = {};
    const int wn =
        MultiByteToWideChar(CP_UTF8, 0, leaf.c_str(), -1, nullptr, 0);
    if (wn <= 1) {
      return false;
    }
    std::wstring leaf_w(static_cast<size_t>(wn - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, leaf.c_str(), -1, leaf_w.data(), wn);
    if (kind == "plugin") {
      if (swprintf_s(a, MAX_PATH, L"..\\data\\plugin\\%s", leaf_w.c_str()) <=
              0 ||
          swprintf_s(bpath, MAX_PATH, L"data\\plugin\\%s", leaf_w.c_str()) <=
              0) {
        return false;
      }
    } else if (kind == "data") {
      if (swprintf_s(a, MAX_PATH, L"..\\data\\%s", leaf_w.c_str()) <= 0 ||
          swprintf_s(bpath, MAX_PATH, L"data\\%s", leaf_w.c_str()) <= 0) {
        return false;
      }
    } else {
      return false;
    }
    const wchar_t* rels[] = {a, bpath};
    return resolve_rel_under_exe(rels, 2, out_path);
  };
  out->capture_path = [](const std::string& leaf, std::string* out_path) {
    if (!out_path || leaf.empty()) {
      return false;
    }
    const int n =
        MultiByteToWideChar(CP_UTF8, 0, leaf.c_str(), -1, nullptr, 0);
    if (n <= 1) {
      return false;
    }
    std::wstring leaf_w(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, leaf.c_str(), -1, leaf_w.data(), n);
    wchar_t path_w[MAX_PATH] = {};
    if (!detail::exe_capture_path(path_w, MAX_PATH, leaf_w.c_str())) {
      return false;
    }
    char utf8[MAX_PATH * 3] = {};
    if (!wide_to_utf8(path_w, utf8, sizeof(utf8))) {
      return false;
    }
    *out_path = utf8;
    return true;
  };
  out->sidecar_path = [](const std::string& rel, std::string* out_path) {
    if (!out_path || rel.empty()) {
      return false;
    }
    char path_a[MAX_PATH] = {};
    if (!detail::exe_sidecar_path_a(path_a, MAX_PATH, rel.c_str())) {
      return false;
    }
    *out_path = path_a;
    return true;
  };
  out->doc_clear = [b]() {
    if (content::MapScene* doc = b->document()) {
      doc->clear();
      doc->clear_style_document();
    }
    return true;
  };
  out->fit_extent = [b]() {
    b->fit_map_extent();
    return true;
  };
  out->export_bmp = [b](const std::string& leaf, const std::string& frame) {
    content::Map2dPresenter* map2d = b->map2d();
    if (!map2d || leaf.empty()) {
      return false;
    }
    const int n =
        MultiByteToWideChar(CP_UTF8, 0, leaf.c_str(), -1, nullptr, 0);
    if (n <= 1) {
      return false;
    }
    std::wstring leaf_w(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, leaf.c_str(), -1, leaf_w.data(), n);
    wchar_t bmp_w[MAX_PATH] = {};
    if (!detail::exe_capture_path(bmp_w, MAX_PATH, leaf_w.c_str())) {
      return false;
    }
    char bmp_a[MAX_PATH] = {};
    if (WideCharToMultiByte(CP_ACP, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                            nullptr) <= 0) {
      return false;
    }
    if (!frame_for_export(*b, frame.empty() ? "document_extent" : frame)) {
      return false;
    }
    content::MapScene* doc = b->document();
    if (!doc || doc->feature_count() == 0) {
      return false;
    }
    // Prefer rebinding so software paint cannot use a stale unbound scene.
    map2d->bind(doc, b->view_frame());
    map2d->invalidate_frame_cache();
    if (ui::views::MapViewport* pane = b->map_viewport()) {
      if (pane->native_view() && IsWindow(pane->native_view())) {
        InvalidateRect(pane->native_view(), nullptr, FALSE);
        UpdateWindow(pane->native_view());
      }
      pane->invalidate_native();
      pane->sync_identity_chrome();
    }
    detail::pump_messages(200);
    return map2d->export_bmp(bmp_a, kExportW, kExportH);
  };
  out->suppress_dialogs = [](bool on) {
    ui::views::Dialog::set_dialog_modals_suppressed_for_test(on);
    return true;
  };
  out->require_plugins = [b]() {
    return b->plugins() && b->plugins()->host() != nullptr;
  };
  out->apply_style_file = [b](const std::string& path_utf8) {
    content::MapScene* doc = b->document();
    if (!doc || path_utf8.empty()) {
      return false;
    }
    std::ifstream in(path_utf8, std::ios::binary);
    if (!in) {
      return false;
    }
    std::string json((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    if (json.empty()) {
      return false;
    }
    auto style = std::make_shared<gis::style::StyleDocument>();
    if (!gis::style::parse_style_document(json, style.get())) {
      return false;
    }
    doc->set_style_document(std::move(style));
    return doc->style_document() != nullptr;
  };
  out->invalidate_map2d = [b]() {
    if (content::Map2dPresenter* map2d = b->map2d()) {
      map2d->invalidate_frame_cache();
    }
    return true;
  };
  out->analysis_set_frame = [b](int index) {
    return b->apply_analysis_frame(index);
  };
  out->analysis_export_frames = [b](const std::string& dir_leaf) {
    return b->export_analysis_frames(dir_leaf);
  };
  out->open_report = [b](const std::string& report_dir) {
    PluginShell* shell = b->plugins();
    if (!shell || !shell->host() || report_dir.empty()) {
      return false;
    }
    return shell->host()->open_report(report_dir);
  };
  out->post_to_report = [b](const std::string& json) {
    PluginShell* shell = b->plugins();
    if (!shell || !shell->host()) {
      return false;
    }
    return shell->host()->post_to_report(json);
  };
}

}  // namespace app
