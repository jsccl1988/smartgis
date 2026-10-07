// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_GIS_HWND_GESTURES_H_
#define CONTENT_BROWSER_GIS_HWND_GESTURES_H_

#include <functional>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "tool/nav/camera_nav.h"

namespace content {

// Touch / trackpad gestures on a GIS viewport HWND:
// - GID_ZOOM / WM_POINTER pinch → zoom
// - GID_PAN (two-finger drag, including left/right) → pan
// Trackpad horizontal wheel stays on MapViewport (WM_MOUSEHWHEEL).
//
// Prefer attach() when the HWND already has a foreign wndproc (Views).
// Prefer bind() + try_handle() when this host owns the wndproc (Cs popup),
// so ComCtl32 v6 SetWindowSubclass is not required.
class GisHwndGestures {
 public:
  using PinchFn = std::function<void(int cursor_x, int cursor_y, double scale)>;
  // Pixel delta in client space (positive dx = content moves right).
  using PanFn = std::function<void(int dx_px, int dy_px)>;
  // Map canvas right-click (client pixels). A small movement still counts as
  // a click; a drag does not open the menu. view.pan does not consume RMB
  // (MapLibre-like browse), so this path owns the navigation context menu.
  using RightClickFn =
      std::function<void(HWND hwnd, int client_x, int client_y)>;
  // true before a pan / zoom gesture or wheel step, false after it.
  using ExtentWatchFn = std::function<void(bool begin)>;
  using ResizeFn = std::function<void()>;

  GisHwndGestures() = default;
  ~GisHwndGestures();

  GisHwndGestures(const GisHwndGestures&) = delete;
  GisHwndGestures& operator=(const GisHwndGestures&) = delete;

  void attach(HWND hwnd, PinchFn on_pinch, PanFn on_pan = {});
  // Same gesture config + callbacks as attach(), without subclassing.
  void bind(HWND hwnd, PinchFn on_pinch, PanFn on_pan = {});
  void detach();
  HWND hwnd() const { return hwnd_; }

  void set_right_click(RightClickFn fn);
  void set_extent_watch(ExtentWatchFn fn);
  void set_viewport_resized(ResizeFn fn);

  // For bind() owners: return true when the message was fully consumed.
  bool try_handle(UINT msg, WPARAM wparam, LPARAM lparam);

 private:
  static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                        LPARAM lparam, UINT_PTR id,
                                        DWORD_PTR data);
  bool on_message(UINT msg, WPARAM wparam, LPARAM lparam);
  bool handle_gesture(LPARAM lparam);
  void handle_pointer(UINT msg, WPARAM wparam, LPARAM lparam);
  void reset_state();
  void configure_hwnd(HWND hwnd, PinchFn on_pinch, PanFn on_pan);
  void clear_callbacks();
  // Returns true when this call opened the sample. Nested calls are ignored.
  bool begin_extent_sample();
  void end_extent_sample();

  HWND hwnd_ = nullptr;
  bool subclassed_ = false;
  // True only after configure_hwnd moved callbacks in. First attach→detach
  // must not tidy default-constructed std::function members (AV under MSVC
  // when the BrowserView object layout was mid-refactor).
  bool has_callbacks_ = false;
  PinchFn on_pinch_;
  PanFn on_pan_;
  RightClickFn on_right_click_;
  ExtentWatchFn on_extent_watch_;
  ResizeFn on_resized_;
  bool extent_open_ = false;
  bool pinch_sampling_ = false;
  bool rdown_ = false;
  int rdown_x_ = 0;
  int rdown_y_ = 0;
  tool::PointerPinchTracker pinch_;
  ULONGLONG last_zoom_arg_ = 0;
  bool zooming_ = false;
  int last_pan_x_ = 0;
  int last_pan_y_ = 0;
  bool panning_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GIS_HWND_GESTURES_H_
