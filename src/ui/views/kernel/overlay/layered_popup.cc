// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/overlay/layered_popup.h"

#include <cmath>

#include "base/core/log.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

namespace ui {
namespace views {

LayeredPopupHost::~LayeredPopupHost() {
  destroy();
}

void LayeredPopupHost::destroy() {
  if (hwnd_ && IsWindow(hwnd_)) {
    DestroyWindow(hwnd_);
  }
  hwnd_ = nullptr;
  surface_ = nullptr;
  layered_ = false;
  placed_valid_ = false;
  placed_x_ = 0;
  placed_y_ = 0;
  placed_w_ = 0;
  placed_h_ = 0;
}

void LayeredPopupHost::note_destroyed(HWND hwnd) {
  if (hwnd_ == hwnd) {
    hwnd_ = nullptr;
    placed_valid_ = false;
  }
}

void LayeredPopupHost::request_paint() {
  if (is_alive()) {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

bool LayeredPopupHost::sync(HWND surface,
                            int frame_x,
                            int frame_y,
                            int frame_w,
                            int frame_h,
                            const Options& opt,
                            void* create_param) {
  if (!surface || !IsWindow(surface) || !opt.class_name || frame_w <= 0 ||
      frame_h <= 0) {
    destroy();
    return false;
  }

  const LONG_PTR ex = GetWindowLongPtrW(surface, GWL_EXSTYLE);
  const bool want_layered = (ex & WS_EX_NOREDIRECTIONBITMAP) != 0;
  const BYTE want_alpha = opt.alpha;
  const bool want_hit_transparent = opt.hit_transparent;

  // Layered path creates an owned WS_POPUP (hWndParent = surface as owner).
  // GetParent on an owned popup returns the owner, not nullptr. Expecting
  // nullptr here destroyed/recreated the eagle-eye (and identity HUD) on
  // every sync_identity_frame tick → visible flicker.
  const HWND expected_anchor = surface;
  if (hwnd_ &&
      (!IsWindow(hwnd_) || layered_ != want_layered ||
       hit_transparent_ != want_hit_transparent ||
       GetParent(hwnd_) != expected_anchor)) {
    LOGGING(LOG_DEBUG,
            "layered_popup: recreate class=%ls parent=%p expect=%p "
            "layered=%d->%d hit_tr=%d->%d",
            opt.class_name, reinterpret_cast<void*>(GetParent(hwnd_)),
            reinterpret_cast<void*>(expected_anchor), layered_ ? 1 : 0,
            want_layered ? 1 : 0, hit_transparent_ ? 1 : 0,
            want_hit_transparent ? 1 : 0);
    destroy();
  }

  surface_ = surface;
  const bool size_changed = placed_valid_ &&
                            (placed_w_ != frame_w || placed_h_ != frame_h);

  int want_x = frame_x;
  int want_y = frame_y;
  if (want_layered) {
    RECT wr = {};
    GetWindowRect(surface, &wr);
    want_x = wr.left + frame_x;
    want_y = wr.top + frame_y;
  }

  bool just_created = false;
  if (!hwnd_) {
    DWORD ex_style = 0;
    DWORD style = 0;
    HWND parent = surface;
    if (want_layered) {
      ex_style = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED;
      if (want_hit_transparent) {
        ex_style |= WS_EX_TRANSPARENT;
      }
      style = WS_POPUP | WS_VISIBLE;
      // Owner for z-order; not a child of the DXGI surface.
      parent = surface;
    } else {
      style = WS_CHILD | WS_VISIBLE;
    }
    hwnd_ = CreateWindowExW(ex_style, opt.class_name, opt.title, style, want_x,
                            want_y, frame_w, frame_h, parent, nullptr,
                            GetModuleHandleW(nullptr), create_param);
    if (!hwnd_) {
      LOGGING(LOG_WARNING, "layered_popup: CreateWindow failed class=%ls",
              opt.class_name);
      return false;
    }
    if (want_layered) {
      SetLayeredWindowAttributes(hwnd_, 0, want_alpha, LWA_ALPHA);
    }
    layered_ = want_layered;
    alpha_ = want_alpha;
    hit_transparent_ = want_hit_transparent;
    placed_x_ = want_x;
    placed_y_ = want_y;
    placed_w_ = frame_w;
    placed_h_ = frame_h;
    placed_valid_ = true;
    just_created = true;
  } else {
    const int slack = opt.geometry_slack_px > 0 ? opt.geometry_slack_px : 0;
    const bool already_visible = IsWindowVisible(hwnd_) != FALSE;
    const bool geometry_same =
        placed_valid_ && std::abs(placed_x_ - want_x) <= slack &&
        std::abs(placed_y_ - want_y) <= slack && placed_w_ == frame_w &&
        placed_h_ == frame_h;
    if (!(geometry_same && already_visible)) {
      UINT flags = SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS | SWP_NOZORDER;
      if (!already_visible) {
        flags |= SWP_SHOWWINDOW;
      }
      SetWindowPos(hwnd_, nullptr, want_x, want_y, frame_w, frame_h, flags);
      placed_x_ = want_x;
      placed_y_ = want_y;
      placed_w_ = frame_w;
      placed_h_ = frame_h;
      placed_valid_ = true;
    } else {
      placed_w_ = frame_w;
      placed_h_ = frame_h;
    }
  }
  return just_created || size_changed;
}

}  // namespace views
}  // namespace ui
