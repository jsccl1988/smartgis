// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/eagle_eye/input.h"

namespace ui {
namespace views {
namespace detail {

EagleEyeInputResult eagle_eye_on_mouse(UINT msg,
                                       int x,
                                       int y,
                                       int frame_w,
                                       int frame_h,
                                       const content::Extent2& world,
                                       content::Extent2* view,
                                       EagleEyeDrag* drag) {
  EagleEyeInputResult out{};
  if (!view || !drag || frame_w <= 0 || frame_h <= 0) {
    return out;
  }

  const EagleEyeRectPx frame{0, 0, frame_w, frame_h};
  const EagleEyeRectPx content = eagle_eye_content_inset(frame);
  const EagleEyeRectPx view_px =
      eagle_eye_view_rect_px(world, *view, content);

  if (msg == WM_LBUTTONDOWN) {
    const EagleEyeHit hit = eagle_eye_hit_test(x, y, frame, view_px);
    if (hit == EagleEyeHit::kNone) {
      return out;
    }
    drag->mode = hit;
    drag->start_view = *view;
    drag->moved = false;
    eagle_eye_px_to_world(world, x, y, content, &drag->anchor_mx,
                          &drag->anchor_my);
    out.handled = true;
    return out;
  }

  if (msg == WM_MOUSEMOVE && drag->mode != EagleEyeHit::kNone) {
    double mx = 0;
    double my = 0;
    if (!eagle_eye_px_to_world(world, x, y, content, &mx, &my)) {
      out.handled = true;
      return out;
    }
    drag->moved = true;
    if (drag->mode == EagleEyeHit::kBox) {
      const double dx = mx - drag->anchor_mx;
      const double dy = my - drag->anchor_my;
      content::Extent2 next = drag->start_view;
      next.xmin += dx;
      next.xmax += dx;
      next.ymin += dy;
      next.ymax += dy;
      *view = eagle_eye_clamp_view_to_world(world, next);
      out.need_paint = true;
    } else if (drag->mode == EagleEyeHit::kHandleNW ||
               drag->mode == EagleEyeHit::kHandleNE ||
               drag->mode == EagleEyeHit::kHandleSW ||
               drag->mode == EagleEyeHit::kHandleSE) {
      *view = eagle_eye_resize_view(world, drag->start_view, drag->mode, mx,
                                    my);
      out.need_paint = true;
    }
    out.handled = true;
    return out;
  }

  if (msg == WM_LBUTTONUP && drag->mode != EagleEyeHit::kNone) {
    const EagleEyeHit mode = drag->mode;
    drag->mode = EagleEyeHit::kNone;
    double mx = 0;
    double my = 0;
    eagle_eye_px_to_world(world, x, y, content, &mx, &my);
    if (mode == EagleEyeHit::kFrame && !drag->moved) {
      out.need_commit = true;
      out.commit_extent = eagle_eye_center_view_on(world, *view, mx, my);
    } else if (mode == EagleEyeHit::kBox || mode == EagleEyeHit::kHandleNW ||
               mode == EagleEyeHit::kHandleNE ||
               mode == EagleEyeHit::kHandleSW ||
               mode == EagleEyeHit::kHandleSE) {
      out.need_commit = true;
      out.commit_extent = *view;
    }
    out.handled = true;
    return out;
  }

  return out;
}

}  // namespace detail
}  // namespace views
}  // namespace ui
