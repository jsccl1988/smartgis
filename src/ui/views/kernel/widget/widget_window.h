// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_WIDGET_WIDGET_WINDOW_H_
#define UI_VIEWS_KERNEL_WIDGET_WIDGET_WINDOW_H_

#include "ui/ui_export.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {

class View;

// CreateWindowEx args derived from Widget::InitParams (placement + styles).
struct WidgetWindowCreate {
  DWORD style = 0;
  DWORD ex_style = 0;
  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  int width = 0;
  int height = 0;
};

// Registers the shared SmartGisViewsWidget class once. |wnd_proc| is Widget's.
UI_EXPORT bool ensure_widget_window_class(WNDPROC wnd_proc);

// Client/DIP sizing → outer CreateWindow box (owned dialog, CSD, or top-level).
UI_EXPORT WidgetWindowCreate compute_widget_window_create(
    const Widget::InitParams& params);

// Creates the HWND; |create_param| is the Widget* passed through WM_NCCREATE.
UI_EXPORT HWND create_widget_hwnd(const WidgetWindowCreate& create,
                                  const Widget::InitParams& params,
                                  void* create_param);

// Owned popups stay above the owner without taking a taskbar slot.
UI_EXPORT void raise_owned_widget_above_owner(HWND hwnd, HWND owner);

// Custom-frame WM_NCHITTEST: resize borders + caption drag vs client controls.
UI_EXPORT LRESULT hit_test_custom_frame(HWND hwnd,
                                        int screen_x,
                                        int screen_y,
                                        float device_scale_factor,
                                        View* contents);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_WIDGET_WIDGET_WINDOW_H_
