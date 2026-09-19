// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_MAP_HWND_GESTURES_H_
#define APP_VIEWS_MAP_HWND_GESTURES_H_

#include <functional>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "tool/camera_nav.h"

namespace app {

// Subclasses a map HWND for pinch zoom (WM_GESTURE GID_ZOOM / WM_POINTER*).
// Two-finger pan stays on MapViewport → TouchMultitouchTracker (WM_POINTER*)
// and trackpad horizontal wheel (WM_MOUSEHWHEEL). Attach is a no-op when the
// HWND cannot take a subclass (then pinch is unavailable).
class MapHwndGestures {
 public:
  using PinchFn = std::function<void(int cursor_x, int cursor_y, double scale)>;

  MapHwndGestures() = default;
  ~MapHwndGestures();

  MapHwndGestures(const MapHwndGestures&) = delete;
  MapHwndGestures& operator=(const MapHwndGestures&) = delete;

  void attach(HWND hwnd, PinchFn on_pinch);
  void detach();
  HWND hwnd() const { return hwnd_; }

 private:
  static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                        LPARAM lparam, UINT_PTR id,
                                        DWORD_PTR data);
  bool on_message(UINT msg, WPARAM wparam, LPARAM lparam);
  bool handle_gesture(LPARAM lparam);
  void handle_pointer(UINT msg, WPARAM wparam, LPARAM lparam);

  HWND hwnd_ = nullptr;
  PinchFn on_pinch_;
  tool::PointerPinchTracker pinch_;
  ULONGLONG last_zoom_arg_ = 0;
  bool zooming_ = false;
};

}  // namespace app

#endif  // APP_VIEWS_MAP_HWND_GESTURES_H_
