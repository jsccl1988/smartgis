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

// Subclasses a map HWND for touch gestures:
// - GID_ZOOM / WM_POINTER pinch → zoom
// - GID_PAN (two-finger drag, including left/right) → pan
// Trackpad horizontal wheel stays on MapViewport (WM_MOUSEHWHEEL).
// Attach is a no-op when the HWND cannot take a subclass.
class MapHwndGestures {
 public:
  using PinchFn = std::function<void(int cursor_x, int cursor_y, double scale)>;
  // Pixel delta in client space (positive dx = content moves right).
  using PanFn = std::function<void(int dx_px, int dy_px)>;

  MapHwndGestures() = default;
  ~MapHwndGestures();

  MapHwndGestures(const MapHwndGestures&) = delete;
  MapHwndGestures& operator=(const MapHwndGestures&) = delete;

  void attach(HWND hwnd, PinchFn on_pinch, PanFn on_pan = {});
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
  PanFn on_pan_;
  tool::PointerPinchTracker pinch_;
  ULONGLONG last_zoom_arg_ = 0;
  bool zooming_ = false;
  int last_pan_x_ = 0;
  int last_pan_y_ = 0;
  bool panning_ = false;
};

}  // namespace app

#endif  // APP_VIEWS_MAP_HWND_GESTURES_H_
