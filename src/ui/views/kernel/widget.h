// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_WIDGET_H_
#define UI_VIEWS_WIDGET_H_

#include <memory>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/view.h"
#include "ui/views/dpi.h"

namespace ui {
namespace views {

// Top-level native HWND that owns a View tree and dispatches input / paint.
class Widget {
 public:
  struct InitParams {
    const wchar_t* title = L"SmartGIS Views";
    int width = 1280;
    int height = 800;
    HWND owner = nullptr;
  };

  Widget();
  ~Widget();

  Widget(const Widget&) = delete;
  Widget& operator=(const Widget&) = delete;

  bool init(const InitParams& params);
  void set_contents_view(std::unique_ptr<View> contents);
  View* contents_view() const { return contents_.get(); }
  HWND hwnd() const { return hwnd_; }

  // Physical pixels per DIP (dpi / 96). Defaults to 1 until init().
  float device_scale_factor() const { return device_scale_factor_; }
  unsigned dpi() const { return dpi_; }

  // Test / programmatic DPI change without a real WM_DPICHANGED.
  void set_device_scale_factor(float scale_factor);

  void show();
  int run_loop();
  // Nested loop until this HWND is destroyed. Does not PostQuitMessage.
  int run_modal();
  void request_close();

  void layout_contents();
  void schedule_paint();

  View* focused_view() const { return focused_; }
  View* hovered_view() const { return hovered_; }
  void set_focused_view(View* view);
  void clear_view_refs(View* view);
  bool advance_focus(bool reverse);

  bool send_mouse(const MouseEvent& event);
  bool send_key(const KeyEvent& event);
  bool send_char(const CharEvent& event);

 private:
  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                   LPARAM lparam);
  static Widget* from_hwnd(HWND hwnd);

  LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  void on_paint();
  void on_size(int width, int height);
  void on_dpi_changed(unsigned new_dpi, const RECT* suggested);
  bool dispatch_mouse(MouseEvent::Type type, WPARAM wparam, LPARAM lparam,
                      int button, int wheel);
  bool dispatch_key(KeyEvent::Type type, WPARAM wparam, LPARAM lparam);
  void track_mouse_leave();
  void update_hover(View* hit);
  void sync_dpi_from_hwnd();

  HWND hwnd_ = nullptr;
  std::unique_ptr<View> contents_;
  View* focused_ = nullptr;
  View* hovered_ = nullptr;
  View* pressed_ = nullptr;
  bool tracking_leave_ = false;
  bool destroying_ = false;
  bool modal_ = false;
  float device_scale_factor_ = 1.f;
  unsigned dpi_ = kDefaultDpi;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_WIDGET_H_
