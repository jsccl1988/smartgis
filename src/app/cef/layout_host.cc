// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/layout_host.h"

#include <shellapi.h>

#include <cstdio>
#include <string>

namespace app {
namespace cef {
namespace {

constexpr wchar_t kClassName[] = L"SmartGisCefLayoutHost";
constexpr wchar_t kWindowTitle[] = L"SmartGIS CEF";

// Fixed IDE chrome proportions (client px after DPI). Map slot sits in the
// center column; CEF paints the full client and sibling map HWNDs cover it.
constexpr int kMenuDip = 36;
constexpr int kStatusDip = 28;
constexpr int kInspectorDip = 160;
constexpr float kCatalogFrac = 0.20f;
constexpr float kMapFrac = 0.55f;

void register_class(HINSTANCE instance) {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = LayoutHost::wnd_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = kClassName;
  RegisterClassExW(&wc);
  done = true;
}

std::wstring module_dir() {
  wchar_t path[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return L".";
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i] = L'\0';
      break;
    }
  }
  return path;
}

}  // namespace

LayoutHost::LayoutHost() = default;

LayoutHost::~LayoutHost() {
  destroy();
}

bool LayoutHost::create(void* instance) {
  HINSTANCE hi = static_cast<HINSTANCE>(instance);
  if (!hi) {
    hi = GetModuleHandleW(nullptr);
  }
  register_class(hi);
  hwnd_ = CreateWindowExW(
      0, kClassName, kWindowTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT,
      CW_USEDEFAULT, 1280, 800, nullptr, nullptr, hi, this);
  if (!hwnd_) {
    return false;
  }
  apply_dpi();
  recompute_layout();
  return true;
}

void LayoutHost::show() {
  if (hwnd_) {
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
  }
}

void LayoutHost::destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

std::wstring LayoutHost::cef_web_index_url() const {
  const std::wstring dir = module_dir();
  std::wstring path = dir + L"\\cef_web\\index.html";
  for (wchar_t& c : path) {
    if (c == L'\\') {
      c = L'/';
    }
  }
  // file:///C:/... requires three slashes after file:
  return L"file:///" + path;
}

void LayoutHost::set_map_slot_rect(const RectPx& r) {
  if (r.w > 0 && r.h > 0) {
    map_slot_rect_ = r;
  }
}

void LayoutHost::set_active_tab(int index) {
  if (index < 0) {
    index = 0;
  }
  if (index > 2) {
    index = 2;
  }
  active_tab_ = index;
}

void LayoutHost::set_resize_callback(ResizeCallback cb, void* user) {
  resize_cb_ = cb;
  resize_user_ = user;
}

void LayoutHost::apply_dpi() {
  if (!hwnd_) {
    dpi_scale_ = 1.f;
    return;
  }
  const UINT dpi = GetDpiForWindow(hwnd_);
  dpi_scale_ = dpi > 0 ? static_cast<float>(dpi) / 96.f : 1.f;
}

void LayoutHost::recompute_layout() {
  if (!hwnd_) {
    return;
  }
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const int cw = rc.right - rc.left;
  const int ch = rc.bottom - rc.top;
  chrome_rect_ = {0, 0, cw > 0 ? cw : 1, ch > 0 ? ch : 1};

  const int menu_h = static_cast<int>(kMenuDip * dpi_scale_);
  const int status_h = static_cast<int>(kStatusDip * dpi_scale_);
  const int inspector_h = static_cast<int>(kInspectorDip * dpi_scale_);
  const int body_top = menu_h;
  const int body_bottom = ch - status_h - inspector_h;
  const int body_h = body_bottom > body_top ? body_bottom - body_top : 1;
  const int catalog_w = static_cast<int>(cw * kCatalogFrac);
  const int map_w = static_cast<int>(cw * kMapFrac);
  map_slot_rect_ = {catalog_w, body_top, map_w > 1 ? map_w : 1,
                    body_h > 1 ? body_h : 1};
}

LRESULT CALLBACK LayoutHost::wnd_proc(HWND hwnd,
                                      UINT msg,
                                      WPARAM wparam,
                                      LPARAM lparam) {
  LayoutHost* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<LayoutHost*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<LayoutHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  switch (msg) {
    case WM_DPICHANGED: {
      if (self) {
        self->apply_dpi();
        const RECT* suggested = reinterpret_cast<RECT*>(lparam);
        if (suggested) {
          SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                       suggested->right - suggested->left,
                       suggested->bottom - suggested->top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
        }
        self->recompute_layout();
        if (self->resize_cb_) {
          self->resize_cb_(self->resize_user_);
        }
      }
      return 0;
    }
    case WM_SIZE:
      if (self) {
        self->recompute_layout();
        if (self->resize_cb_) {
          self->resize_cb_(self->resize_user_);
        }
      }
      return 0;
    case WM_DESTROY:
      if (self) {
        self->hwnd_ = nullptr;
      }
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace cef
}  // namespace app
