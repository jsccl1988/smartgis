// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/panels/debug_console_composer.h"
#include "app/views/ui/browser_view.h"

#include "app/views/browser/browser.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/il.runtime/backend/mark.h"
#include "app/views/il.runtime/backend/run_script.h"
#include "app/views/util/charset.h"
#include "base/process/switches.h"
#include "content/browser/debug/debug_agent.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/map_layer_types.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/views/kernel/shell/event.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {

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

}  // namespace

// Diagnostic tools / debug-agent host binding.

// Diagnostic Tools / DebugAgent horizon wire.
DebugConsoleComposer::DebugConsoleComposer(BrowserView* host) : host_(host) {}

void DebugConsoleComposer::bind_debug_agent_host() {
  if (!host_->browser_) {
    return;
  }
  host_->bind_gis_python_bridge();
  content::DebugAgentHost host;
  host.refresh_map = [this] {
    if (host_->browser_) {
      host_->browser_->on_view_command("view.refresh", -1, false, 0, 0);
    }
  };
  host.extent_string = [this] {
    if (!host_->browser_) {
      return std::string("no browser");
    }
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    if (!host_->browser_->document()->compute_extent(&min_x, &min_y, &max_x,
                                              &max_y)) {
      return std::string("empty extent");
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.6f,%.6f,%.6f,%.6f", min_x, min_y, max_x,
                  max_y);
    return std::string(buf);
  };
  host.layer_names = [this] {
    std::vector<std::string> names;
    if (!host_->browser_ || !host_->browser_->document()) {
      return names;
    }
    std::function<void(const content::LayerDesc&)> walk =
        [&](const content::LayerDesc& d) {
          std::string row = d.name.empty() ? d.id : d.name;
          if (row.empty()) {
            row = "(unnamed)";
          }
          if (!d.visible) {
            row += " [hidden]";
          }
          if (d.active) {
            row += " [active]";
          }
          names.push_back(std::move(row));
          for (const content::LayerDesc& child : d.children) {
            walk(child);
          }
        };
    for (const content::LayerDesc& d :
         host_->browser_->document()->layer_descs()) {
      walk(d);
    }
    if (names.empty()) {
      names.push_back("(no layers)");
    }
    return names;
  };
  host.ui_find = [this](const std::string& name) {
    ui::views::View* root = host_->widget_.contents_view();
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
  host.ui_click = [this](int x, int y, int button) {
    ui::views::MouseEvent down;
    down.type = ui::views::MouseEvent::Type::kDown;
    down.x = x;
    down.y = y;
    down.button = button > 0 ? button : 1;
    host_->widget_.send_mouse(down);
    ui::views::MouseEvent up = down;
    up.type = ui::views::MouseEvent::Type::kUp;
    host_->widget_.send_mouse(up);
    return std::string("clicked");
  };
  host.ui_type = [this](const std::string& utf8) {
    const std::wstring wide = detail::utf8_to_wide(utf8);
    for (wchar_t ch : wide) {
      ui::views::CharEvent ev;
      ev.ch = ch;
      host_->widget_.send_char(ev);
    }
    return std::string("typed ") + std::to_string(wide.size()) + " chars";
  };
  host.ui_dump_tree = [this]() {
    ui::views::View* root = host_->widget_.contents_view();
    if (!root) {
      return std::string("not wired");
    }
    std::ostringstream oss;
    append_view_tree_lines(root, 0, oss);
    return oss.str();
  };
  host.ui_overlay_stats = [this]() {
    std::ostringstream oss;
    oss << '{';
    ui::views::DrawHost* pane = host_->active_map();
    if (pane) {
      oss << "\"hud_fps\":" << pane->hud_fps()
          << ",\"gpu_present_ok\":"
          << (pane->last_gpu_present_ok() ? "true" : "false")
          << ",\"content_present_ok\":"
          << (pane->last_content_present_ok() ? "true" : "false")
          << ",\"attach_mode\":" << static_cast<int>(pane->attach_mode());
    } else {
      oss << "\"hud_fps\":null,\"gpu_present_ok\":null,"
             "\"content_present_ok\":null,\"attach_mode\":null";
    }
    if (host_->browser_ && host_->browser_->map2d()) {
      content::Map2dPresenter* map2d = host_->browser_->map2d();
      oss << ",\"map2d_gpu_present_ok\":"
          << (map2d->last_gpu_present_ok() ? "true" : "false")
          << ",\"map2d_gpu_drew\":"
          << (map2d->last_gpu_present_drew() ? "true" : "false")
          << ",\"map2d_layout_builds\":" << map2d->layout_build_count()
          << ",\"map2d_reused_layout\":"
          << (map2d->last_present_reused_layout() ? "true" : "false");
    } else {
      oss << ",\"map2d_gpu_present_ok\":null";
    }
    oss << '}';
    return oss.str();
  };
  host.ui_capture_shell = [this](const std::string& path_utf8) {
    HWND hwnd = host_->widget_.hwnd();
    if (!hwnd || !IsWindow(hwnd)) {
      return std::string("error: no hwnd");
    }
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
      return std::string("error: empty client");
    }
    std::filesystem::path out_path = path_utf8.empty()
                                         ? (std::filesystem::path("out") /
                                            "ui_forensics" / "capture.bmp")
                                         : std::filesystem::path(path_utf8);
    std::error_code ec;
    if (out_path.has_parent_path()) {
      std::filesystem::create_directories(out_path.parent_path(), ec);
    }
    HDC hdc_win = GetDC(hwnd);
    HDC mem = CreateCompatibleDC(hdc_win);
    HBITMAP bmp = CreateCompatibleBitmap(hdc_win, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, hdc_win, 0, 0, SRCCOPY);

    BITMAPFILEHEADER bfh = {};
    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = w;
    bih.biHeight = -h;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    const DWORD img_bytes = static_cast<DWORD>(w) * static_cast<DWORD>(h) * 4u;
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + img_bytes;

    std::vector<std::uint8_t> pixels(img_bytes);
    BITMAPINFO bi = {};
    bi.bmiHeader = bih;
    GetDIBits(mem, bmp, 0, static_cast<UINT>(h), pixels.data(), &bi,
              DIB_RGB_COLORS);

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, hdc_win);

    std::ofstream file(out_path, std::ios::binary);
    if (!file) {
      return std::string("error: open failed");
    }
    file.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    file.write(reinterpret_cast<const char*>(&bih), sizeof(bih));
    file.write(reinterpret_cast<const char*>(pixels.data()),
               static_cast<std::streamsize>(pixels.size()));
    return out_path.string();
  };
  host.py_eval = [this](const std::string& code) {
    if (!host_->browser_ || !host_->browser_->plugins()) {
      return std::string("error: no plugin shell");
    }
    return host_->browser_->plugins()->eval_python(code);
  };
  host.script_run = [this](const std::string& path_utf8) {
    if (!host_->browser_) {
      return std::string("error: no browser");
    }
    return app::run_execution_script_utf8(*host_->browser_, path_utf8,
                                           app::detail::kUiShowcaseMarkLeaf);
  };
  content::debug_agent().set_host(std::move(host));
}


void DebugConsoleComposer::wire_debug_console() {
  if (!host_->diagnostic_tools_) {
    return;
  }
  host_->diagnostic_tools_->set_console_submit([this](const std::string& line) {
    host_->bind_debug_agent_host();
    if (!content::debug_agent().is_running()) {
      content::debug_agent().start();
    }
    const std::string out = content::debug_agent().exec_line(line);
    if (host_->diagnostic_tools_ && host_->diagnostic_tools_->console_pane() &&
        !out.empty()) {
      host_->diagnostic_tools_->console_pane()->append_line(out);
    }
  });
  // Plugin / map2d / atmosphere harness sets skip-ambox-catalog before
  // init_shell. Eager bind_gis_python_bridge → Browser::plugins() has AVd
  // under page-heap IFEO when shell_ui .obj layout is skewed (unique_ptr::get
  // on a freefill plugins_ slot). Defer agent bind until Console submit.
  const char* skip = base::switch_cstr("skip-ambox-catalog");
  if (skip && skip[0] != '\0' && skip[0] != '0') {
    return;
  }
  // Shell defaults Diagnostic Tools open (Console tab); bind agent so :cmd
  // works without requiring View → Toggle first.
  if (host_->diagnostic_tools_->is_tools_visible()) {
    host_->bind_debug_agent_host();
    if (!content::debug_agent().is_running()) {
      content::debug_agent().start();
    }
  }
}


void DebugConsoleComposer::toggle_debug_console() {
  if (!host_->diagnostic_tools_) {
    return;
  }
  const bool next = !host_->diagnostic_tools_->is_tools_visible();
  if (next) {
    host_->bind_debug_agent_host();
    content::debug_agent().start();
  }
  host_->diagnostic_tools_->set_visible_tools(next);
  host_->set_status_message(next ? "Diagnostic Tools on" : "Diagnostic Tools off");
  if (ui::views::View* root = host_->contents_view()) {
    root->schedule_paint();
  }
}


}  // namespace app
