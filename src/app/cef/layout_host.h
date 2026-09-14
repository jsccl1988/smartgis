// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_CEF_LAYOUT_HOST_H_
#define APP_CEF_LAYOUT_HOST_H_

#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace cef {

// Pixel rectangle in the top-level client coordinate space.
struct RectPx {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  bool is_valid() const { return w > 0 && h > 0; }
};

// Top-level Win32 frame: owns the outer HWND, chrome (CEF) bounds, and the
// active map-slot rectangle used by sibling CefMapSlot HWNDs.
class LayoutHost {
 public:
  LayoutHost();
  ~LayoutHost();

  LayoutHost(const LayoutHost&) = delete;
  LayoutHost& operator=(const LayoutHost&) = delete;

  bool create(void* instance);
  void show();
  void destroy();

  HWND hwnd() const { return hwnd_; }
  RectPx chrome_rect() const { return chrome_rect_; }
  RectPx map_slot_rect() const { return map_slot_rect_; }
  float dpi_scale() const { return dpi_scale_; }
  std::wstring cef_web_index_url() const;

  void set_map_slot_rect(const RectPx& r);
  void set_active_tab(int index);
  int active_tab() const { return active_tab_; }

  using ResizeCallback = void (*)(void* user);
  void set_resize_callback(ResizeCallback cb, void* user);

  static LRESULT CALLBACK wnd_proc(HWND hwnd,
                                   UINT msg,
                                   WPARAM wparam,
                                   LPARAM lparam);

 private:
  void recompute_layout();
  void apply_dpi();

  HWND hwnd_ = nullptr;
  RectPx chrome_rect_;
  RectPx map_slot_rect_;
  float dpi_scale_ = 1.f;
  int active_tab_ = 0;
  ResizeCallback resize_cb_ = nullptr;
  void* resize_user_ = nullptr;
};

}  // namespace cef
}  // namespace app

#endif  // APP_CEF_LAYOUT_HOST_H_
