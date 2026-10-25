// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_OVERLAY_LAYERED_POPUP_H_
#define UI_VIEWS_KERNEL_OVERLAY_LAYERED_POPUP_H_

#include "ui/ui_export.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {

// Child HWND or DXGI-safe layered popup synced to a surface client rect.
// Tracks last *requested* geometry (SWP_ASYNCWINDOWPOS lags GetWindowRect);
// skips redundant SetWindowPos / SWP_SHOWWINDOW to avoid chrome and IME flash.
class UI_EXPORT LayeredPopupHost {
 public:
  // Create / recreate options. Caller owns WNDCLASS registration for
  // |class_name| (and its WNDPROC).
  struct Options {
    const wchar_t* class_name = nullptr;
    const wchar_t* title = L"";
    BYTE alpha = 255;
    // WS_EX_TRANSPARENT — hits fall through (identity HUD).
    bool hit_transparent = false;
    // Pixel slack when comparing requested vs last place (DXGI/DWM jitter).
    int geometry_slack_px = 1;
  };

  LayeredPopupHost() = default;
  ~LayeredPopupHost();

  LayeredPopupHost(const LayeredPopupHost&) = delete;
  LayeredPopupHost& operator=(const LayeredPopupHost&) = delete;

  void destroy();

  HWND hwnd() const { return hwnd_; }
  // Last |surface| passed to sync (owner for layered popup / parent for child).
  HWND surface() const { return surface_; }
  bool layered() const { return layered_; }
  int width() const { return placed_w_; }
  int height() const { return placed_h_; }
  bool is_alive() const { return hwnd_ != nullptr && IsWindow(hwnd_); }

  // Place |frame| (x,y,w,h) in |surface| client coordinates. When |surface|
  // has WS_EX_NOREDIRECTIONBITMAP, creates a WS_POPUP layered window in screen
  // space; otherwise a WS_CHILD. |create_param| is CREATESTRUCT::lpCreateParams.
  // Returns true if the window was created or its size changed (invalidate).
  bool sync(HWND surface,
            int frame_x,
            int frame_y,
            int frame_w,
            int frame_h,
            const Options& opt,
            void* create_param);

  void request_paint();

  // Clear hwnd_ if |hwnd| matches (call from owner's WM_DESTROY).
  void note_destroyed(HWND hwnd);

 private:
  HWND hwnd_ = nullptr;
  HWND surface_ = nullptr;
  bool layered_ = false;
  int placed_x_ = 0;
  int placed_y_ = 0;
  int placed_w_ = 0;
  int placed_h_ = 0;
  bool placed_valid_ = false;
  BYTE alpha_ = 255;
  bool hit_transparent_ = false;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_OVERLAY_LAYERED_POPUP_H_
