// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/eagle_eye/eagle_eye_host.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "base/core/log.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>
#include <shlobj.h>

namespace ui {
namespace views {
namespace {

constexpr wchar_t kEagleEyeClass[] = L"SmartGisMapEagleEye";

std::filesystem::path eagle_eye_persist_path() {
  wchar_t* folder = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder)) ||
      !folder) {
    return {};
  }
  std::filesystem::path path(folder);
  CoTaskMemFree(folder);
  path /= L"SmartGIS";
  path /= L"eagle_eye_visible.txt";
  return path;
}

}  // namespace

bool EagleEyeHost::load_persisted_visible() {
  const auto path = eagle_eye_persist_path();
  if (path.empty()) {
    return true;
  }
  std::ifstream in(path);
  if (!in) {
    return true;
  }
  std::string line;
  if (!std::getline(in, line)) {
    return true;
  }
  return !(line == "0" || line == "false" || line == "off");
}

void EagleEyeHost::persist_visible(bool visible) {
  const auto path = eagle_eye_persist_path();
  if (path.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    return;
  }
  out << (visible ? "1" : "0");
}

void EagleEyeHost::register_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = EagleEyeHost::wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kEagleEyeClass;
  RegisterClassExW(&wc);
  done = true;
}

EagleEyeHost::EagleEyeHost() {
  visible_ = load_persisted_visible();
}

EagleEyeHost::~EagleEyeHost() {
  destroy();
}

void EagleEyeHost::set_visible(bool visible) {
  if (visible_ == visible) {
    return;
  }
  visible_ = visible;
  persist_visible(visible_);
  if (!visible_) {
    destroy();
  } else if (surface_ && IsWindow(surface_)) {
    sync_to_surface(surface_);
  }
}

void EagleEyeHost::toggle_visible() {
  set_visible(!visible_);
}

void EagleEyeHost::set_world_extent(const content::Extent2& world) {
  if (!detail::eagle_eye_extent_nonempty(world)) {
    return;
  }
  world_ = world;
  view_ = detail::eagle_eye_clamp_view_to_world(world_, view_);
  if (hwnd_ && IsWindow(hwnd_)) {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void EagleEyeHost::set_view_extent(const content::Extent2& view) {
  if (!detail::eagle_eye_extent_nonempty(view)) {
    return;
  }
  view_ = detail::eagle_eye_clamp_view_to_world(world_, view);
  if (hwnd_ && IsWindow(hwnd_)) {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void EagleEyeHost::destroy() {
  if (hwnd_ && IsWindow(hwnd_)) {
    DestroyWindow(hwnd_);
  }
  hwnd_ = nullptr;
  layered_popup_ = false;
  frame_w_ = 0;
  frame_h_ = 0;
}

detail::EagleEyeRectPx EagleEyeHost::content_rect() const {
  detail::EagleEyeRectPx frame{0, 0, frame_w_, frame_h_};
  return detail::eagle_eye_content_inset(frame);
}

void EagleEyeHost::sync_to_surface(HWND surface) {
  surface_ = surface;
  if (!visible_ || !surface || !IsWindow(surface)) {
    destroy();
    return;
  }
  RECT parent_rc = {};
  GetClientRect(surface, &parent_rc);
  const int parent_w = parent_rc.right;
  const int parent_h = parent_rc.bottom;
  const detail::EagleEyeRectPx frame =
      detail::eagle_eye_frame_on_parent(parent_w, parent_h);
  if (frame.w <= 0 || frame.h <= 0) {
    destroy();
    return;
  }

  const LONG_PTR ex = GetWindowLongPtrW(surface, GWL_EXSTYLE);
  const bool want_layered =
      (ex & WS_EX_NOREDIRECTIONBITMAP) != 0;

  register_class();
  if (hwnd_ &&
      (!IsWindow(hwnd_) || layered_popup_ != want_layered ||
       GetParent(hwnd_) != (want_layered ? nullptr : surface))) {
    destroy();
  }

  frame_w_ = frame.w;
  frame_h_ = frame.h;
  layered_popup_ = want_layered;

  constexpr UINT kAsyncPos =
      SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS | SWP_SHOWWINDOW | SWP_NOZORDER;

  if (!hwnd_) {
    if (want_layered) {
      RECT wr = {};
      GetWindowRect(surface, &wr);
      const int screen_x = wr.left + frame.x;
      const int screen_y = wr.top + frame.y;
      hwnd_ = CreateWindowExW(
          WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED, kEagleEyeClass,
          L"Eagle Eye", WS_POPUP | WS_VISIBLE, screen_x, screen_y, frame.w,
          frame.h, surface, nullptr, GetModuleHandleW(nullptr), this);
      if (hwnd_) {
        SetLayeredWindowAttributes(hwnd_, 0, 230, LWA_ALPHA);
      }
    } else {
      hwnd_ = CreateWindowExW(0, kEagleEyeClass, L"Eagle Eye",
                              WS_CHILD | WS_VISIBLE, frame.x, frame.y, frame.w,
                              frame.h, surface, nullptr,
                              GetModuleHandleW(nullptr), this);
    }
    if (!hwnd_) {
      LOGGING(LOG_WARNING, "eagle_eye: CreateWindow failed");
      return;
    }
  } else {
    if (want_layered) {
      RECT wr = {};
      GetWindowRect(surface, &wr);
      SetWindowPos(hwnd_, nullptr, wr.left + frame.x, wr.top + frame.y, frame.w,
                   frame.h, kAsyncPos);
    } else {
      SetWindowPos(hwnd_, nullptr, frame.x, frame.y, frame.w, frame.h,
                   kAsyncPos);
    }
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void EagleEyeHost::commit_view(const content::Extent2& next) {
  content::Extent2 clamped =
      detail::eagle_eye_clamp_view_to_world(world_, next);
  if (!detail::eagle_eye_extent_nonempty(clamped)) {
    return;
  }
  view_ = clamped;
  if (commit_fn_) {
    commit_fn_(view_);
  }
  if (hwnd_ && IsWindow(hwnd_)) {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void EagleEyeHost::paint(HDC hdc) {
  RECT rc = {0, 0, frame_w_, frame_h_};
  HBRUSH bg = CreateSolidBrush(RGB(42, 48, 58));
  FillRect(hdc, &rc, bg);
  DeleteObject(bg);

  HPEN border = CreatePen(PS_SOLID, 1, RGB(90, 110, 140));
  HGDIOBJ old_pen = SelectObject(hdc, border);
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  Rectangle(hdc, 0, 0, frame_w_, frame_h_);

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(200, 210, 220));
  TextOutW(hdc, 6, 2, L"Overview", 8);

  const detail::EagleEyeRectPx content = content_rect();
  if (content.w > 0 && content.h > 0) {
    RECT world_rc = {content.x, content.y, content.x + content.w,
                     content.y + content.h};
    HBRUSH world_brush = CreateSolidBrush(RGB(58, 72, 88));
    FillRect(hdc, &world_rc, world_brush);
    DeleteObject(world_brush);

    // Simplified land silhouette: inset fill suggesting mainland coverage.
    const int inset = (std::max)(4, content.w / 16);
    RECT land = {content.x + inset, content.y + inset,
                 content.x + content.w - inset, content.y + content.h - inset};
    HBRUSH land_brush = CreateSolidBrush(RGB(72, 96, 78));
    FillRect(hdc, &land, land_brush);
    DeleteObject(land_brush);

    const detail::EagleEyeRectPx view_px =
        detail::eagle_eye_view_rect_px(world_, view_, content);
    if (view_px.w > 0 && view_px.h > 0) {
      HPEN view_pen = CreatePen(PS_SOLID, 2, RGB(255, 196, 64));
      SelectObject(hdc, view_pen);
      Rectangle(hdc, view_px.x, view_px.y, view_px.x + view_px.w,
                view_px.y + view_px.h);
      DeleteObject(view_pen);

      HBRUSH handle = CreateSolidBrush(RGB(255, 220, 100));
      const int hs = detail::kEagleEyeHandlePx;
      auto draw_handle = [&](int hx, int hy) {
        RECT hr = {hx - hs / 2, hy - hs / 2, hx + hs / 2 + 1, hy + hs / 2 + 1};
        FillRect(hdc, &hr, handle);
      };
      draw_handle(view_px.x, view_px.y);
      draw_handle(view_px.x + view_px.w, view_px.y);
      draw_handle(view_px.x, view_px.y + view_px.h);
      draw_handle(view_px.x + view_px.w, view_px.y + view_px.h);
      DeleteObject(handle);
    }
  }

  SelectObject(hdc, old_pen);
  SelectObject(hdc, old_brush);
  DeleteObject(border);
}

bool EagleEyeHost::on_mouse(UINT msg, int x, int y) {
  const detail::EagleEyeRectPx frame{0, 0, frame_w_, frame_h_};
  const detail::EagleEyeRectPx content = content_rect();
  const detail::EagleEyeRectPx view_px =
      detail::eagle_eye_view_rect_px(world_, view_, content);

  if (msg == WM_LBUTTONDOWN) {
    const detail::EagleEyeHit hit =
        detail::eagle_eye_hit_test(x, y, frame, view_px);
    if (hit == detail::EagleEyeHit::kNone) {
      return false;
    }
    SetCapture(hwnd_);
    drag_mode_ = hit;
    drag_start_view_ = view_;
    drag_moved_ = false;
    detail::eagle_eye_px_to_world(world_, x, y, content, &drag_anchor_mx_,
                                  &drag_anchor_my_);
    return true;
  }

  if (msg == WM_MOUSEMOVE && drag_mode_ != detail::EagleEyeHit::kNone) {
    double mx = 0;
    double my = 0;
    if (!detail::eagle_eye_px_to_world(world_, x, y, content, &mx, &my)) {
      return true;
    }
    drag_moved_ = true;
    if (drag_mode_ == detail::EagleEyeHit::kBox) {
      const double dx = mx - drag_anchor_mx_;
      const double dy = my - drag_anchor_my_;
      content::Extent2 next = drag_start_view_;
      next.xmin += dx;
      next.xmax += dx;
      next.ymin += dy;
      next.ymax += dy;
      view_ = detail::eagle_eye_clamp_view_to_world(world_, next);
      InvalidateRect(hwnd_, nullptr, FALSE);
    } else if (drag_mode_ == detail::EagleEyeHit::kHandleNW ||
               drag_mode_ == detail::EagleEyeHit::kHandleNE ||
               drag_mode_ == detail::EagleEyeHit::kHandleSW ||
               drag_mode_ == detail::EagleEyeHit::kHandleSE) {
      view_ = detail::eagle_eye_resize_view(world_, drag_start_view_,
                                            drag_mode_, mx, my);
      InvalidateRect(hwnd_, nullptr, FALSE);
    }
    return true;
  }

  if (msg == WM_LBUTTONUP && drag_mode_ != detail::EagleEyeHit::kNone) {
    const detail::EagleEyeHit mode = drag_mode_;
    drag_mode_ = detail::EagleEyeHit::kNone;
    if (GetCapture() == hwnd_) {
      ReleaseCapture();
    }
    double mx = 0;
    double my = 0;
    detail::eagle_eye_px_to_world(world_, x, y, content, &mx, &my);
    if (mode == detail::EagleEyeHit::kFrame && !drag_moved_) {
      commit_view(detail::eagle_eye_center_view_on(world_, view_, mx, my));
    } else if (mode == detail::EagleEyeHit::kBox ||
               mode == detail::EagleEyeHit::kHandleNW ||
               mode == detail::EagleEyeHit::kHandleNE ||
               mode == detail::EagleEyeHit::kHandleSW ||
               mode == detail::EagleEyeHit::kHandleSE) {
      commit_view(view_);
    }
    return true;
  }

  return false;
}

LRESULT CALLBACK EagleEyeHost::wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                        LPARAM lparam) {
  EagleEyeHost* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<EagleEyeHost*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<EagleEyeHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps = {};
      HDC hdc = BeginPaint(hwnd, &ps);
      self->paint(hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE:
      if (self->on_mouse(msg, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))) {
        return 0;
      }
      break;
    case WM_DESTROY:
      if (self->hwnd_ == hwnd) {
        self->hwnd_ = nullptr;
      }
      break;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace views
}  // namespace ui
