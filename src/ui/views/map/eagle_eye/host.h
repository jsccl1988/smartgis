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
#include "ui/views/kernel/overlay/layered_popup.h"
#include "ui/views/map/eagle_eye/geom.h"
#include "ui/views/map/eagle_eye/input.h"

namespace ui {
namespace views {

// Floating 2D overview over a map surface. Owns a LayeredPopupHost; paint /
// persist / gesture math live in sibling TUs.
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

  void sync_to_surface(HWND surface);
  void destroy();

  HWND hwnd() const { return popup_.hwnd(); }

  static bool load_persisted_visible();
  static void persist_visible(bool visible);

 private:
  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                   LPARAM lparam);
  static void register_class();

  void commit_view(const content::Extent2& next);

  ExtentCommitFn commit_fn_;
  content::Extent2 world_{73.0, 18.0, 135.0, 54.0};
  content::Extent2 view_{80.0, 20.0, 128.0, 48.0};
  bool visible_ = true;
  HWND surface_ = nullptr;
  LayeredPopupHost popup_;
  detail::EagleEyeDrag drag_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_EAGLE_EYE_HOST_H_
