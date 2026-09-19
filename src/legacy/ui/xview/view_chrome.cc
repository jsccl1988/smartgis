// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"

#include "legacy/ui/xview/view_chrome.h"

#include <unordered_map>
#include <utility>

#include "content/public/map_types.h"
#include "content/public/view_host.h"
#include "tool/camera_nav.h"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

namespace ui {
namespace {

constexpr wchar_t kGestureProp[] = L"SmtGestureCfg";

tool::PointerPinchTracker g_pinch;

// Midpoint of PT_TOUCH contacts → multitouch InputEvent (Views / CEF contract).
struct MidpointTracker {
  bool on_down(uint32_t id, int32_t x, int32_t y, content::InputEvent* out) {
    contacts_[id] = {x, y};
    if (contacts_.size() < 2) {
      return false;
    }
    multitouch_ = true;
    fill(content::InputEvent::Kind::kLDown, out);
    return true;
  }

  bool on_move(uint32_t id, int32_t x, int32_t y, content::InputEvent* out) {
    auto it = contacts_.find(id);
    if (it == contacts_.end()) {
      return false;
    }
    it->second = {x, y};
    if (!multitouch_ || contacts_.size() < 2) {
      return false;
    }
    fill(content::InputEvent::Kind::kMouseMove, out);
    return true;
  }

  bool on_up(uint32_t id, int32_t x, int32_t y, content::InputEvent* out) {
    contacts_.erase(id);
    if (!multitouch_) {
      return false;
    }
    if (contacts_.size() >= 2) {
      fill(content::InputEvent::Kind::kMouseMove, out);
      return true;
    }
    // Last multitouch lift: emit up at the departing contact.
    out->kind = content::InputEvent::Kind::kLUp;
    out->flags = 0;
    out->x_px = x;
    out->y_px = y;
    out->wheel = 0;
    out->key = 0;
    out->t_qpc = 0;
    out->pointer_count = 2;
    multitouch_ = false;
    contacts_.clear();
    return true;
  }

  bool suppress_mouse() const { return multitouch_; }
  size_t contact_count() const { return contacts_.size(); }

  void clear() {
    contacts_.clear();
    multitouch_ = false;
  }

 private:
  bool midpoint(int32_t* x, int32_t* y) const {
    if (contacts_.size() < 2) {
      return false;
    }
    int64_t sx = 0;
    int64_t sy = 0;
    for (const auto& c : contacts_) {
      sx += c.second.first;
      sy += c.second.second;
    }
    *x = static_cast<int32_t>(sx / static_cast<int64_t>(contacts_.size()));
    *y = static_cast<int32_t>(sy / static_cast<int64_t>(contacts_.size()));
    return true;
  }

  void fill(content::InputEvent::Kind kind, content::InputEvent* out) const {
    int32_t x = 0;
    int32_t y = 0;
    midpoint(&x, &y);
    out->kind = kind;
    out->flags = 0;
    out->x_px = x;
    out->y_px = y;
    out->wheel = 0;
    out->key = 0;
    out->t_qpc = 0;
    out->pointer_count = static_cast<uint32_t>(contacts_.size());
  }

  std::unordered_map<uint32_t, std::pair<int32_t, int32_t>> contacts_;
  bool multitouch_ = false;
};

MidpointTracker g_midpoint;

struct GesturePanState {
  bool active = false;
  int last_x = 0;
  int last_y = 0;
};

GesturePanState g_gesture_pan;
ULONGLONG g_last_zoom_arg = 0;
bool g_zooming = false;

bool dispatch_event(content::ViewHost* host, content::InputEvent e) {
  return host && host->dispatch_input(e);
}

void ensure_gesture_config(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  if (GetPropW(hwnd, kGestureProp)) {
    return;
  }
  // Pinch zoom + two-finger pan. Do not enable single-finger pan flags.
  GESTURECONFIG gc[2] = {};
  gc[0].dwID = GID_ZOOM;
  gc[0].dwWant = GC_ZOOM;
  gc[0].dwBlock = 0;
  gc[1].dwID = GID_PAN;
  gc[1].dwWant = GC_PAN | GC_PAN_WITH_INERTIA;
  gc[1].dwBlock = 0;
  if (SetGestureConfig(hwnd, 0, 2, gc, sizeof(GESTURECONFIG))) {
    SetPropW(hwnd, kGestureProp, reinterpret_cast<HANDLE>(1));
  }
}

bool handle_gesture(content::ViewHost* host, HWND hwnd, LPARAM lparam) {
  GESTUREINFO gi = {};
  gi.cbSize = sizeof(gi);
  const auto handle = reinterpret_cast<HGESTUREINFO>(lparam);
  if (!GetGestureInfo(handle, &gi)) {
    return false;
  }

  POINT pt = {gi.ptsLocation.x, gi.ptsLocation.y};
  if (hwnd) {
    ScreenToClient(hwnd, &pt);
  }

  if (gi.dwID == GID_PAN) {
    // Two-finger drag (including left/right) → multitouch pan InputEvents.
    if (gi.dwFlags & GF_BEGIN) {
      g_gesture_pan.active = true;
      g_gesture_pan.last_x = pt.x;
      g_gesture_pan.last_y = pt.y;
      g_midpoint.clear();
      g_pinch.reset();
      content::InputEvent e{};
      e.kind = content::InputEvent::Kind::kLDown;
      e.x_px = pt.x;
      e.y_px = pt.y;
      e.pointer_count = 2;
      dispatch_event(host, e);
    } else if (g_gesture_pan.active) {
      content::InputEvent e{};
      e.kind = content::InputEvent::Kind::kMouseMove;
      e.x_px = pt.x;
      e.y_px = pt.y;
      e.pointer_count = 2;
      dispatch_event(host, e);
      g_gesture_pan.last_x = pt.x;
      g_gesture_pan.last_y = pt.y;
    }
    if (gi.dwFlags & GF_END) {
      content::InputEvent e{};
      e.kind = content::InputEvent::Kind::kLUp;
      e.x_px = pt.x;
      e.y_px = pt.y;
      e.pointer_count = 2;
      dispatch_event(host, e);
      g_gesture_pan.active = false;
    }
    CloseGestureInfoHandle(handle);
    return true;
  }

  if (gi.dwID == GID_ZOOM) {
    if (gi.dwFlags & GF_BEGIN) {
      g_last_zoom_arg = gi.ullArguments;
      g_zooming = true;
    } else if (g_zooming && g_last_zoom_arg > 0 && gi.ullArguments > 0 &&
               host) {
      const double scale = static_cast<double>(gi.ullArguments) /
                           static_cast<double>(g_last_zoom_arg);
      g_last_zoom_arg = gi.ullArguments;
      const int32_t wheel = tool::scale_to_wheel_delta(scale);
      if (wheel != 0) {
        content::InputEvent e{};
        e.kind = content::InputEvent::Kind::kWheel;
        e.wheel = wheel;
        e.x_px = pt.x;
        e.y_px = pt.y;
        dispatch_event(host, e);
      }
    }
    if (gi.dwFlags & GF_END) {
      g_zooming = false;
      g_last_zoom_arg = 0;
    }
    CloseGestureInfoHandle(handle);
    return true;
  }

  CloseGestureInfoHandle(handle);
  return true;
}

bool handle_pointer(content::ViewHost* host, HWND hwnd, UINT message,
                    WPARAM wparam, LPARAM lparam) {
#if defined(WM_POINTERDOWN)
  // GID_PAN owns the two-finger drag — do not stack pinch / midpoint pan.
  if (g_gesture_pan.active) {
    return true;
  }

  const UINT id = GET_POINTERID_WPARAM(wparam);
  POINTER_INPUT_TYPE pointer_type = PT_POINTER;
  if (GetPointerType(id, &pointer_type) && pointer_type != PT_TOUCH) {
    return false;
  }

  POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
  if (hwnd) {
    ScreenToClient(hwnd, &pt);
  }

  if (message == WM_POINTERDOWN) {
    g_pinch.on_down(id, pt.x, pt.y);
    content::InputEvent sample{};
    if (g_midpoint.on_down(id, pt.x, pt.y, &sample)) {
      return host && host->dispatch_input(sample);
    }
    return g_midpoint.suppress_mouse() || g_pinch.contact_count() >= 2;
  }

  if (message == WM_POINTERUPDATE) {
    // Prefer midpoint pan; also apply pinch when finger distance changes.
    content::InputEvent sample{};
    bool consumed = false;
    if (g_midpoint.on_move(id, pt.x, pt.y, &sample) && host) {
      consumed = host->dispatch_input(sample);
    }
    double scale = 0;
    if (g_pinch.on_move(id, pt.x, pt.y, &scale) && host) {
      content::InputEvent e{};
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = tool::scale_to_wheel_delta(scale);
      e.x_px = pt.x;
      e.y_px = pt.y;
      if (e.wheel != 0) {
        consumed = host->dispatch_input(e) || consumed;
      }
    }
    return consumed || g_midpoint.suppress_mouse() ||
           g_pinch.contact_count() >= 2;
  }

  if (message == WM_POINTERUP || message == WM_POINTERCAPTURECHANGED ||
      message == WM_POINTERLEAVE) {
    g_pinch.on_up(id);
    content::InputEvent sample{};
    if (g_midpoint.on_up(id, pt.x, pt.y, &sample) && host) {
      return host->dispatch_input(sample);
    }
    return true;
  }
#else
  (void)host;
  (void)hwnd;
  (void)message;
  (void)wparam;
  (void)lparam;
#endif
  return false;
}

}  // namespace

bool dispatch_chrome_message(content::ViewHost* host, UINT message,
                             WPARAM wparam, LPARAM lparam) {
  return dispatch_chrome_message(host, nullptr, message, wparam, lparam);
}

bool dispatch_chrome_message(content::ViewHost* host, HWND hwnd, UINT message,
                             WPARAM wparam, LPARAM lparam) {
  if (!host) {
    return false;
  }
  if (hwnd) {
    ensure_gesture_config(hwnd);
  }
  if (message == WM_GESTURE) {
    return handle_gesture(host, hwnd, lparam);
  }
#if defined(WM_POINTERDOWN)
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE ||
      message == WM_POINTERUP || message == WM_POINTERCAPTURECHANGED ||
      message == WM_POINTERLEAVE) {
    return handle_pointer(host, hwnd, message, wparam, lparam);
  }
#endif
  content::InputEvent e{};
  switch (message) {
    case WM_MOUSEMOVE:
      e.kind = content::InputEvent::Kind::kMouseMove;
      break;
    case WM_LBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kLDown;
      break;
    case WM_LBUTTONUP:
      e.kind = content::InputEvent::Kind::kLUp;
      break;
    case WM_LBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kLDClick;
      break;
    case WM_RBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kRDown;
      break;
    case WM_RBUTTONUP:
      e.kind = content::InputEvent::Kind::kRUp;
      break;
    case WM_RBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kRDClick;
      break;
    case WM_MBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kRDown;
      e.flags = MK_MBUTTON;
      break;
    case WM_MBUTTONUP:
      e.kind = content::InputEvent::Kind::kRUp;
      e.flags = MK_MBUTTON;
      break;
    case WM_MOUSEWHEEL: {
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = static_cast<int32_t>(GET_WHEEL_DELTA_WPARAM(wparam));
      e.flags = static_cast<uint32_t>(GET_KEYSTATE_WPARAM(wparam));
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      if (hwnd) {
        ScreenToClient(hwnd, &pt);
      }
      e.x_px = pt.x;
      e.y_px = pt.y;
      if (e.flags & MK_SHIFT) {
        e.wheel = -e.wheel;
      }
      return host->dispatch_input(e);
    }
    case WM_MOUSEHWHEEL: {
      // Trackpad / mouse horizontal wheel → pan (kHorizontalWheel).
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = static_cast<int32_t>(GET_WHEEL_DELTA_WPARAM(wparam));
      e.flags = static_cast<uint32_t>(GET_KEYSTATE_WPARAM(wparam)) |
                content::input_flags::kHorizontalWheel;
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      if (hwnd) {
        ScreenToClient(hwnd, &pt);
      }
      e.x_px = pt.x;
      e.y_px = pt.y;
      return host->dispatch_input(e);
    }
    case WM_KEYDOWN:
      e.kind = content::InputEvent::Kind::kKeyDown;
      e.key = static_cast<uint32_t>(wparam);
      return host->dispatch_input(e);
    default:
      return false;
  }
  e.x_px = static_cast<int32_t>(static_cast<short>(LOWORD(lparam)));
  e.y_px = static_cast<int32_t>(static_cast<short>(HIWORD(lparam)));
  if (e.flags == 0) {
    e.flags = static_cast<uint32_t>(wparam);
  }
  if (g_midpoint.suppress_mouse() || g_gesture_pan.active) {
    // Swallow primary-contact mouse synthesis during multitouch / GID_PAN.
    return true;
  }
  return dispatch_event(host, e);
}

}  // namespace ui
