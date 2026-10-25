// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/eagle_eye/host.h"

#include "ui/views/map/eagle_eye/paint.h"
#include "ui/views/map/eagle_eye/persist.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

namespace ui {
namespace views {
namespace {

constexpr wchar_t kEagleEyeClass[] = L"SmartGisMapEagleEye";

}  // namespace

bool EagleEyeHost::load_persisted_visible() {
  return detail::eagle_eye_load_visible();
}

void EagleEyeHost::persist_visible(bool visible) {
  detail::eagle_eye_persist_visible(visible);
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
  if (detail::eagle_eye_extent_equal(world_, world)) {
    return;
  }
  world_ = world;
  view_ = detail::eagle_eye_clamp_view_to_world(world_, view_);
  popup_.request_paint();
}

void EagleEyeHost::set_view_extent(const content::Extent2& view) {
  if (!detail::eagle_eye_extent_nonempty(view)) {
    return;
  }
  content::Extent2 clamped =
      detail::eagle_eye_clamp_view_to_world(world_, view);
  if (detail::eagle_eye_extent_equal(view_, clamped)) {
    return;
  }
  view_ = clamped;
  popup_.request_paint();
}

void EagleEyeHost::destroy() {
  popup_.destroy();
}

void EagleEyeHost::sync_to_surface(HWND surface) {
  surface_ = surface;
  if (!visible_ || !surface || !IsWindow(surface)) {
    destroy();
    return;
  }
  RECT parent_rc = {};
  GetClientRect(surface, &parent_rc);
  const detail::EagleEyeRectPx frame =
      detail::eagle_eye_frame_on_parent(parent_rc.right, parent_rc.bottom);
  if (frame.w <= 0 || frame.h <= 0) {
    destroy();
    return;
  }

  register_class();
  LayeredPopupHost::Options opt;
  opt.class_name = kEagleEyeClass;
  opt.title = L"Eagle Eye";
  opt.alpha = 230;
  opt.hit_transparent = false;
  const bool need_paint =
      popup_.sync(surface, frame.x, frame.y, frame.w, frame.h, opt, this);
  if (need_paint) {
    popup_.request_paint();
  }
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
  popup_.request_paint();
}

LRESULT CALLBACK EagleEyeHost::wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                        LPARAM lparam) {
  EagleEyeHost* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<EagleEyeHost*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self =
        reinterpret_cast<EagleEyeHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps = {};
      HDC hdc = BeginPaint(hwnd, &ps);
      detail::eagle_eye_paint(hdc, self->popup_.width(), self->popup_.height(),
                              self->world_, self->view_);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE: {
      const detail::EagleEyeInputResult r = detail::eagle_eye_on_mouse(
          msg, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
          self->popup_.width(), self->popup_.height(), self->world_,
          &self->view_, &self->drag_);
      if (msg == WM_LBUTTONDOWN && r.handled) {
        SetCapture(hwnd);
      }
      if (msg == WM_LBUTTONUP && r.handled && GetCapture() == hwnd) {
        ReleaseCapture();
      }
      if (r.need_paint) {
        self->popup_.request_paint();
      }
      if (r.need_commit) {
        self->commit_view(r.commit_extent);
      }
      if (r.handled) {
        return 0;
      }
      break;
    }
    case WM_DESTROY:
      self->popup_.note_destroyed(hwnd);
      break;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace views
}  // namespace ui
