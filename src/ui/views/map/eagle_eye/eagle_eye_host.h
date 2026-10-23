// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_EAGLE_EYE_HOST_H_
#define UI_VIEWS_MAP_EAGLE_EYE_HOST_H_

#include <functional>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/public/types.h"
#include "ui/views/map/eagle_eye/eagle_eye_geom.h"

namespace ui {
namespace views {

// Floating 2D overview (eagle eye) over a map surface HWND. Paints a simplified
// world box + main-view rectangle; click / drag / corner-resize commits a new
// view extent via |commit_fn|.
class EagleEyeHost {
 public:
  using ExtentCommitFn = std::function<void(const content::Extent2& extent)>;

  EagleEyeHost();
  ~EagleEyeHost();

  EagleEyeHost(const EagleEyeHost&) = delete;
  EagleEyeHost& operator=(const EagleEyeHost&) = delete;

  void set_commit_fn(ExtentCommitFn fn) { commit_fn_ = std::move(fn); }

  void set_visible(bool visible);
  bool visible() const { return visible_; }
  void toggle_visible();

  void set_world_extent(const content::Extent2& world);
  void set_view_extent(const content::Extent2& view);
  const content::Extent2& world_extent() const { return world_; }
  const content::Extent2& view_extent() const { return view_; }

  // Create / move / destroy the overlay relative to |surface| client size.
  // For DXGI NOREDIRECTION parents, uses a layered popup (same pattern as
  // identity HUD) so painting and hit-testing work above the swapchain.
  void sync_to_surface(HWND surface);
  void destroy();

  HWND hwnd() const { return hwnd_; }

  static bool load_persisted_visible();
  static void persist_visible(bool visible);

 private:
  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                   LPARAM lparam);
  static void register_class();

  void paint(HDC hdc);
  bool on_mouse(UINT msg, int x, int y);
  void commit_view(const content::Extent2& next);
  detail::EagleEyeRectPx content_rect() const;

  ExtentCommitFn commit_fn_;
  content::Extent2 world_{73.0, 18.0, 135.0, 54.0};
  content::Extent2 view_{80.0, 20.0, 128.0, 48.0};
  bool visible_ = true;
  HWND hwnd_ = nullptr;
  HWND surface_ = nullptr;
  bool layered_popup_ = false;
  int frame_w_ = 0;
  int frame_h_ = 0;

  detail::EagleEyeHit drag_mode_ = detail::EagleEyeHit::kNone;
  content::Extent2 drag_start_view_{};
  double drag_anchor_mx_ = 0;
  double drag_anchor_my_ = 0;
  bool drag_moved_ = false;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_EAGLE_EYE_HOST_H_
