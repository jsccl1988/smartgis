// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_EAGLE_EYE_INPUT_H_
#define UI_VIEWS_MAP_EAGLE_EYE_INPUT_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/public/types.h"
#include "ui/views/map/eagle_eye/geom.h"

namespace ui {
namespace views {
namespace detail {

// Drag / click state for the overview view-box.
struct EagleEyeDrag {
  EagleEyeHit mode = EagleEyeHit::kNone;
  content::Extent2 start_view{};
  double anchor_mx = 0;
  double anchor_my = 0;
  bool moved = false;
};

struct EagleEyeInputResult {
  bool handled = false;
  bool need_paint = false;
  bool need_commit = false;
  content::Extent2 commit_extent{};
};

// Pure gesture math (no HWND). Host applies capture / Invalidate / commit_fn.
EagleEyeInputResult eagle_eye_on_mouse(UINT msg,
                                       int x,
                                       int y,
                                       int frame_w,
                                       int frame_h,
                                       const content::Extent2& world,
                                       content::Extent2* view,
                                       EagleEyeDrag* drag);

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_EAGLE_EYE_INPUT_H_
