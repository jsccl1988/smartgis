// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/showcase/ui/interact/interact_json.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/self_test/self_test.h"
#include "content/public/map_layer_types.h"
#include "content/public/view_host.h"
#include <rapidjson/document.h>
#include "ui/gis/catalog/catalog_view.h"
#include "ui/views/primitives/collection/tab_strip.h"

namespace app {
namespace detail {
namespace {

void script_mark(const char* token) {
  write_mark(kUiShowcaseMarkLeaf, token, /*truncate=*/false);
}

bool read_file_utf8(const std::wstring& path, std::string* out) {
  if (!out) {
    return false;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) {
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  const long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz < 0 || sz > 4 * 1024 * 1024) {
    std::fclose(f);
    return false;
  }
  out->resize(static_cast<size_t>(sz));
  const size_t n = std::fread(out->data(), 1, out->size(), f);
  std::fclose(f);
  out->resize(n);
  return n > 0;
}

bool step_allows_inproc(const rapidjson::Value& step) {
  if (!step.HasMember("drivers") || !step["drivers"].IsArray()) {
    return true;
  }
  for (const auto& d : step["drivers"].GetArray()) {
    if (d.IsString() && std::strcmp(d.GetString(), "inproc") == 0) {
      return true;
    }
  }
  return false;
}

WORD vk_from_name(const char* name) {
  if (!name) {
    return 0;
  }
  if (std::strcmp(name, "ESCAPE") == 0 || std::strcmp(name, "Esc") == 0) {
    return VK_ESCAPE;
  }
  if (std::strcmp(name, "RETURN") == 0 || std::strcmp(name, "ENTER") == 0) {
    return VK_RETURN;
  }
  if (std::strcmp(name, "TAB") == 0) {
    return VK_TAB;
  }
  if (std::strcmp(name, "SPACE") == 0) {
    return VK_SPACE;
  }
  if (name[0] && !name[1]) {
    const char c = name[0];
    if (c >= 'A' && c <= 'Z') {
      return static_cast<WORD>(c);
    }
    if (c >= 'a' && c <= 'z') {
      return static_cast<WORD>(c - 'a' + 'A');
    }
    if (c >= '0' && c <= '9') {
      return static_cast<WORD>(c);
    }
  }
  return 0;
}

LPARAM client_lparam(int x, int y) {
  return MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y));
}

bool post_mouse(HWND hwnd, UINT down, UINT up, int x, int y) {
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  const LPARAM lp = client_lparam(x, y);
  PostMessageW(hwnd, WM_MOUSEMOVE, 0, lp);
  PostMessageW(hwnd, down, (down == WM_RBUTTONDOWN) ? MK_RBUTTON : MK_LBUTTON, lp);
  PostMessageW(hwnd, up, 0, lp);
  return true;
}

bool post_drag(HWND hwnd, int x0, int y0, int x1, int y1) {
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  PostMessageW(hwnd, WM_MOUSEMOVE, 0, client_lparam(x0, y0));
  PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, client_lparam(x0, y0));
  PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, client_lparam(x1, y1));
  PostMessageW(hwnd, WM_LBUTTONUP, 0, client_lparam(x1, y1));
  return true;
}

bool post_wheel(HWND hwnd, int x, int y, int delta) {
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  POINT pt = {x, y};
  ClientToScreen(hwnd, &pt);
  const WPARAM wp = MAKEWPARAM(0, static_cast<short>(delta));
  const LPARAM lp = MAKELPARAM(static_cast<WORD>(pt.x), static_cast<WORD>(pt.y));
  PostMessageW(hwnd, WM_MOUSEWHEEL, wp, lp);
  return true;
}

bool dispatch_map_click(Browser& browser, int x, int y, bool right) {
  content::ViewHost* host = browser.edit_view_host();
  if (!host) {
    return false;
  }
  content::InputEvent e{};
  e.kind = right ? content::InputEvent::Kind::kRDown
                 : content::InputEvent::Kind::kLDown;
  e.x_px = x;
  e.y_px = y;
  return host->dispatch_input(e);
}

bool run_step(Browser& browser, const rapidjson::Value& step);

bool post_path(HWND hwnd, const rapidjson::Value& points) {
  if (!hwnd || !IsWindow(hwnd) || !points.IsArray() || points.Size() < 2) {
    return false;
  }
  const auto& p0 = points[0];
  if (!p0.IsArray() || p0.Size() < 2 || !p0[0].IsInt() || !p0[1].IsInt()) {
    return false;
  }
  const int x0 = p0[0].GetInt();
  const int y0 = p0[1].GetInt();
  PostMessageW(hwnd, WM_MOUSEMOVE, 0, client_lparam(x0, y0));
  PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, client_lparam(x0, y0));
  for (rapidjson::SizeType i = 1; i < points.Size(); ++i) {
    const auto& pt = points[i];
    if (!pt.IsArray() || pt.Size() < 2 || !pt[0].IsInt() || !pt[1].IsInt()) {
      continue;
    }
    PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
                 client_lparam(pt[0].GetInt(), pt[1].GetInt()));
  }
  const auto& last = points[points.Size() - 1];
  const int x1 = last[0].GetInt();
  const int y1 = last[1].GetInt();
  PostMessageW(hwnd, WM_LBUTTONUP, 0, client_lparam(x1, y1));
  return true;
}

bool map_path(Browser& browser, const rapidjson::Value& points) {
  content::ViewHost* host = browser.edit_view_host();
  if (!host || !points.IsArray() || points.Size() < 2) {
    return false;
  }
  const auto& p0 = points[0];
  if (!p0.IsArray() || p0.Size() < 2) {
    return false;
  }
  content::InputEvent down{};
  down.kind = content::InputEvent::Kind::kLDown;
  down.x_px = p0[0].GetInt();
  down.y_px = p0[1].GetInt();
  if (!host->dispatch_input(down)) {
    return false;
  }
  for (rapidjson::SizeType i = 1; i < points.Size(); ++i) {
    const auto& pt = points[i];
    if (!pt.IsArray() || pt.Size() < 2) {
      continue;
    }
    content::InputEvent move{};
    move.kind = content::InputEvent::Kind::kMouseMove;
    move.x_px = pt[0].GetInt();
    move.y_px = pt[1].GetInt();
    if (!host->dispatch_input(move)) {
      return false;
    }
  }
  const auto& last = points[points.Size() - 1];
  content::InputEvent up{};
  up.kind = content::InputEvent::Kind::kLUp;
  up.x_px = last[0].GetInt();
  up.y_px = last[1].GetInt();
  return host->dispatch_input(up);
}

bool run_steps_array(Browser& browser, const rapidjson::Value& steps) {
  if (!steps.IsArray()) {
    return false;
  }
  for (const auto& child : steps.GetArray()) {
    if (!run_step(browser, child)) {
      return false;
    }
  }
  return true;
}

WORD parse_modifier_vk(const char* name) {
  if (!name) {
    return 0;
  }
  if (std::strcmp(name, "CTRL") == 0 || std::strcmp(name, "CONTROL") == 0) {
    return VK_CONTROL;
  }
  if (std::strcmp(name, "SHIFT") == 0) {
    return VK_SHIFT;
  }
  if (std::strcmp(name, "ALT") == 0 || std::strcmp(name, "MENU") == 0) {
    return VK_MENU;
  }
  return vk_from_name(name);
}

bool run_step(Browser& browser, const rapidjson::Value& step) {
  if (!step.IsObject() || !step.HasMember("op") || !step["op"].IsString()) {
    return false;
  }
  if (!step_allows_inproc(step)) {
    return true;
  }
  const char* op = step["op"].GetString();
  HWND hwnd = browser.hwnd();

  if (std::strcmp(op, "pump") == 0) {
    const int ms = step.HasMember("ms") && step["ms"].IsInt() ? step["ms"].GetInt()
                                                              : 100;
    pump_views_messages(static_cast<DWORD>(ms > 0 ? ms : 0));
    return true;
  }
  if (std::strcmp(op, "select_map_tab") == 0) {
    const int index =
        step.HasMember("index") && step["index"].IsInt() ? step["index"].GetInt()
                                                         : 0;
    browser.select_map_tab(index);
    return true;
  }
  if (std::strcmp(op, "catalog_tab") == 0) {
    const int index =
        step.HasMember("index") && step["index"].IsInt() ? step["index"].GetInt()
                                                         : 0;
    if (ui::views::CatalogView* cat = browser.catalog_view()) {
      if (ui::views::TabStrip* tabs = cat->source_tabs()) {
        tabs->set_active(index);
      }
    }
    return true;
  }
  if (std::strcmp(op, "inspector_tab") == 0) {
    const int index =
        step.HasMember("index") && step["index"].IsInt() ? step["index"].GetInt()
                                                         : 0;
    if (browser.ui()) {
      browser.ui()->activate_inspector_tab(index);
    }
    return true;
  }
  if (std::strcmp(op, "mark") == 0) {
    if (step.HasMember("name") && step["name"].IsString()) {
      script_mark(step["name"].GetString());
    }
    return true;
  }
  if (std::strcmp(op, "window") == 0) {
    const char* action =
        step.HasMember("action") && step["action"].IsString()
            ? step["action"].GetString()
            : "";
    if (!hwnd) {
      return false;
    }
    if (std::strcmp(action, "activate") == 0) {
      ShowWindow(hwnd, SW_SHOW);
      SetForegroundWindow(hwnd);
      return true;
    }
    if (std::strcmp(action, "resize") == 0) {
      const int w =
          step.HasMember("w") && step["w"].IsInt() ? step["w"].GetInt() : 1280;
      const int h =
          step.HasMember("h") && step["h"].IsInt() ? step["h"].GetInt() : 800;
      if (IsZoomed(hwnd) || IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
      }
      SetWindowPos(hwnd, nullptr, 0, 0, w > 0 ? w : 1280, h > 0 ? h : 800,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);
      return true;
    }
    if (std::strcmp(action, "move") == 0) {
      const int x =
          step.HasMember("x") && step["x"].IsInt() ? step["x"].GetInt() : 0;
      const int y =
          step.HasMember("y") && step["y"].IsInt() ? step["y"].GetInt() : 0;
      RECT rc = {};
      GetWindowRect(hwnd, &rc);
      MoveWindow(hwnd, x, y, rc.right - rc.left, rc.bottom - rc.top, TRUE);
      return true;
    }
    return true;
  }
  if (std::strcmp(op, "key") == 0) {
    WORD vk = 0;
    if (step.HasMember("vk")) {
      if (step["vk"].IsString()) {
        vk = vk_from_name(step["vk"].GetString());
      } else if (step["vk"].IsInt()) {
        vk = static_cast<WORD>(step["vk"].GetInt());
      }
    }
    if (!vk || !hwnd) {
      return false;
    }
    PostMessageW(hwnd, WM_KEYDOWN, vk, 0);
    PostMessageW(hwnd, WM_KEYUP, vk, 0);
    return true;
  }
  // Continuous composition: nested steps / loops.
  if (std::strcmp(op, "seq") == 0) {
    if (!step.HasMember("steps")) {
      return false;
    }
    return run_steps_array(browser, step["steps"]);
  }
  if (std::strcmp(op, "repeat") == 0) {
    const int count =
        step.HasMember("count") && step["count"].IsInt() ? step["count"].GetInt()
                                                         : 1;
    if (!step.HasMember("steps") || count < 1) {
      return false;
    }
    for (int i = 0; i < count; ++i) {
      if (!run_steps_array(browser, step["steps"])) {
        return false;
      }
    }
    return true;
  }
  // Hold modifiers, run nested steps, then release (chorded click/drag).
  if (std::strcmp(op, "chord") == 0) {
    if (!hwnd || !step.HasMember("steps")) {
      return false;
    }
    std::vector<WORD> mods;
    if (step.HasMember("modifiers") && step["modifiers"].IsArray()) {
      for (const auto& m : step["modifiers"].GetArray()) {
        if (!m.IsString()) {
          continue;
        }
        const WORD vk = parse_modifier_vk(m.GetString());
        if (vk) {
          mods.push_back(vk);
        }
      }
    }
    for (WORD vk : mods) {
      PostMessageW(hwnd, WM_KEYDOWN, vk, 0);
    }
    const bool ok = run_steps_array(browser, step["steps"]);
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) {
      PostMessageW(hwnd, WM_KEYUP, *it, 0);
    }
    return ok;
  }
  // Multi-point LMB path (continuous drag polyline).
  if (std::strcmp(op, "path") == 0) {
    if (!step.HasMember("points") || !step["points"].IsArray()) {
      return false;
    }
    const char* path_target =
        step.HasMember("target") && step["target"].IsString()
            ? step["target"].GetString()
            : "shell_client";
    if (std::strcmp(path_target, "map_client") == 0) {
      if (map_path(browser, step["points"])) {
        return true;
      }
    }
    return post_path(hwnd, step["points"]);
  }
  // N pan strokes: down → move(+dx,+dy) → up, with pump between.
  if (std::strcmp(op, "pan_burst") == 0) {
    const int count =
        step.HasMember("count") && step["count"].IsInt() ? step["count"].GetInt()
                                                         : 4;
    const int x =
        step.HasMember("x") && step["x"].IsInt() ? step["x"].GetInt() : 200;
    const int y =
        step.HasMember("y") && step["y"].IsInt() ? step["y"].GetInt() : 300;
    const int dx =
        step.HasMember("dx") && step["dx"].IsInt() ? step["dx"].GetInt() : 40;
    const int dy =
        step.HasMember("dy") && step["dy"].IsInt() ? step["dy"].GetInt() : 24;
    const int between =
        step.HasMember("pump_ms") && step["pump_ms"].IsInt()
            ? step["pump_ms"].GetInt()
            : 40;
    const char* burst_target =
        step.HasMember("target") && step["target"].IsString()
            ? step["target"].GetString()
            : "map_client";
    const bool use_map = std::strcmp(burst_target, "map_client") == 0;
    for (int i = 0; i < count; ++i) {
      const int x0 = x + (i % 5) * 8;
      const int y0 = y + (i % 7) * 6;
      const int x1 = x0 + dx;
      const int y1 = y0 + dy;
      if (use_map) {
        content::ViewHost* host = browser.edit_view_host();
        if (host) {
          content::InputEvent down{};
          down.kind = content::InputEvent::Kind::kLDown;
          down.x_px = x0;
          down.y_px = y0;
          content::InputEvent move{};
          move.kind = content::InputEvent::Kind::kMouseMove;
          move.x_px = x1;
          move.y_px = y1;
          content::InputEvent up{};
          up.kind = content::InputEvent::Kind::kLUp;
          up.x_px = x1;
          up.y_px = y1;
          if (!host->dispatch_input(down) || !host->dispatch_input(move) ||
              !host->dispatch_input(up)) {
            return false;
          }
        } else if (!post_drag(hwnd, x0, y0, x1, y1)) {
          return false;
        }
      } else if (!post_drag(hwnd, x0, y0, x1, y1)) {
        return false;
      }
      if (between > 0) {
        pump_views_messages(static_cast<DWORD>(between));
      }
    }
    return true;
  }
  if (std::strcmp(op, "wheel_burst") == 0) {
    const int count =
        step.HasMember("count") && step["count"].IsInt() ? step["count"].GetInt()
                                                         : 3;
    const int x =
        step.HasMember("x") && step["x"].IsInt() ? step["x"].GetInt() : 400;
    const int y =
        step.HasMember("y") && step["y"].IsInt() ? step["y"].GetInt() : 400;
    const int delta =
        step.HasMember("delta") && step["delta"].IsInt() ? step["delta"].GetInt()
                                                         : -120;
    const int between =
        step.HasMember("pump_ms") && step["pump_ms"].IsInt()
            ? step["pump_ms"].GetInt()
            : 50;
    const char* burst_target =
        step.HasMember("target") && step["target"].IsString()
            ? step["target"].GetString()
            : "map_client";
    const bool use_map = std::strcmp(burst_target, "map_client") == 0;
    for (int i = 0; i < count; ++i) {
      const int d = (i % 2 == 0) ? delta : -delta;
      if (use_map) {
        content::ViewHost* host = browser.edit_view_host();
        if (host) {
          content::InputEvent e{};
          e.kind = content::InputEvent::Kind::kWheel;
          e.x_px = x;
          e.y_px = y;
          e.wheel = d;
          if (!host->dispatch_input(e)) {
            return false;
          }
        } else if (!post_wheel(hwnd, x, y, d)) {
          return false;
        }
      } else if (!post_wheel(hwnd, x, y, d)) {
        return false;
      }
      if (between > 0) {
        pump_views_messages(static_cast<DWORD>(between));
      }
    }
    return true;
  }

  const char* target =
      step.HasMember("target") && step["target"].IsString()
          ? step["target"].GetString()
          : "shell_client";
  const bool map = std::strcmp(target, "map_client") == 0;

  if (std::strcmp(op, "click") == 0 || std::strcmp(op, "rclick") == 0 ||
      std::strcmp(op, "dblclick") == 0) {
    const int x = step.HasMember("x") && step["x"].IsInt() ? step["x"].GetInt() : 0;
    const int y = step.HasMember("y") && step["y"].IsInt() ? step["y"].GetInt() : 0;
    const bool right =
        std::strcmp(op, "rclick") == 0 ||
        (step.HasMember("button") && step["button"].IsString() &&
         std::strcmp(step["button"].GetString(), "right") == 0);
    if (map) {
      if (!dispatch_map_click(browser, x, y, right)) {
        return post_mouse(hwnd, right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN,
                          right ? WM_RBUTTONUP : WM_LBUTTONUP, x, y);
      }
      if (std::strcmp(op, "dblclick") == 0) {
        dispatch_map_click(browser, x, y, right);
      }
      return true;
    }
    if (std::strcmp(op, "dblclick") == 0) {
      post_mouse(hwnd, WM_LBUTTONDOWN, WM_LBUTTONUP, x, y);
      return post_mouse(hwnd, WM_LBUTTONDBLCLK, WM_LBUTTONUP, x, y);
    }
    return post_mouse(hwnd, right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN,
                      right ? WM_RBUTTONUP : WM_LBUTTONUP, x, y);
  }
  if (std::strcmp(op, "drag") == 0) {
    const int x0 =
        step.HasMember("x0") && step["x0"].IsInt() ? step["x0"].GetInt() : 0;
    const int y0 =
        step.HasMember("y0") && step["y0"].IsInt() ? step["y0"].GetInt() : 0;
    const int x1 =
        step.HasMember("x1") && step["x1"].IsInt() ? step["x1"].GetInt() : x0;
    const int y1 =
        step.HasMember("y1") && step["y1"].IsInt() ? step["y1"].GetInt() : y0;
    if (map) {
      content::ViewHost* host = browser.edit_view_host();
      if (host) {
        content::InputEvent down{};
        down.kind = content::InputEvent::Kind::kLDown;
        down.x_px = x0;
        down.y_px = y0;
        content::InputEvent move{};
        move.kind = content::InputEvent::Kind::kMouseMove;
        move.x_px = x1;
        move.y_px = y1;
        content::InputEvent up{};
        up.kind = content::InputEvent::Kind::kLUp;
        up.x_px = x1;
        up.y_px = y1;
        return host->dispatch_input(down) && host->dispatch_input(move) &&
               host->dispatch_input(up);
      }
    }
    return post_drag(hwnd, x0, y0, x1, y1);
  }
  if (std::strcmp(op, "wheel") == 0) {
    const int x = step.HasMember("x") && step["x"].IsInt() ? step["x"].GetInt() : 0;
    const int y = step.HasMember("y") && step["y"].IsInt() ? step["y"].GetInt() : 0;
    const int delta =
        step.HasMember("delta") && step["delta"].IsInt() ? step["delta"].GetInt()
                                                         : -120;
    if (map) {
      content::ViewHost* host = browser.edit_view_host();
      if (host) {
        content::InputEvent e{};
        e.kind = content::InputEvent::Kind::kWheel;
        e.x_px = x;
        e.y_px = y;
        e.wheel = delta;
        return host->dispatch_input(e);
      }
    }
    return post_wheel(hwnd, x, y, delta);
  }
  // Unknown ops: skip rather than fail the showcase.
  std::fprintf(stderr, "interact-script: skip unknown op '%s'\n", op);
  return true;
}

bool apply_ui_interact_json_impl(Browser& browser, const std::wstring& path) {
  std::string json;
  if (!read_file_utf8(path, &json)) {
    std::fwprintf(stderr, L"interact-script: failed to read %ls\n", path.c_str());
    return false;
  }
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("steps") ||
      !doc["steps"].IsArray()) {
    std::fprintf(stderr, "interact-script: invalid JSON steps\n");
    return false;
  }
  script_mark("script-load-ok");
  for (const auto& step : doc["steps"].GetArray()) {
    if (!run_step(browser, step)) {
      script_mark("script-step-fail");
      std::fprintf(stderr, "interact-script: step failed\n");
      return false;
    }
  }
  script_mark("script-done");
  return true;
}

}  // namespace

bool apply_ui_interact_json(Browser& browser, const std::wstring& path) {
  return apply_ui_interact_json_impl(browser, path);
}

}  // namespace detail
}  // namespace app
