// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_EAGLE_EYE_GEOM_H_
#define UI_VIEWS_MAP_EAGLE_EYE_GEOM_H_

#include <algorithm>
#include <cmath>

#include "content/public/types.h"

namespace ui {
namespace views {
namespace detail {

// Pixel rect inside the eagle-eye window client (inclusive origin, exclusive
// size semantics for width/height).
struct EagleEyeRectPx {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

inline bool eagle_eye_extent_nonempty(const content::Extent2& e) {
  return e.xmax > e.xmin && e.ymax > e.ymin;
}

// Bottom-right inset on the map surface. Sized ~22% of the short side, clamped.
inline EagleEyeRectPx eagle_eye_frame_on_parent(int parent_w, int parent_h) {
  EagleEyeRectPx out{};
  if (parent_w < 80 || parent_h < 80) {
    return out;
  }
  const int margin = 12;
  const int short_side = (std::min)(parent_w, parent_h);
  int side = static_cast<int>(short_side * 0.22);
  side = (std::max)(120, (std::min)(side, 280));
  int box_w = side;
  int box_h = static_cast<int>(side * 0.72);
  box_h = (std::max)(90, (std::min)(box_h, 200));
  if (box_w + margin * 2 > parent_w) {
    box_w = parent_w - margin * 2;
  }
  if (box_h + margin * 2 > parent_h) {
    box_h = parent_h - margin * 2;
  }
  if (box_w < 64 || box_h < 48) {
    return out;
  }
  out.w = box_w;
  out.h = box_h;
  out.x = parent_w - margin - box_w;
  out.y = parent_h - margin - box_h;
  return out;
}

// Content area inside the chrome border (padding for title strip + border).
inline EagleEyeRectPx eagle_eye_content_inset(const EagleEyeRectPx& frame) {
  EagleEyeRectPx c = frame;
  constexpr int kPad = 6;
  constexpr int kTitle = 16;
  c.x += kPad;
  c.y += kPad + kTitle;
  c.w -= kPad * 2;
  c.h -= kPad * 2 + kTitle;
  if (c.w < 8 || c.h < 8) {
    c.w = 0;
    c.h = 0;
  }
  return c;
}

inline bool eagle_eye_world_to_px(const content::Extent2& world,
                                 double mx,
                                 double my,
                                 const EagleEyeRectPx& content,
                                 double* out_x,
                                 double* out_y) {
  if (!out_x || !out_y || !eagle_eye_extent_nonempty(world) || content.w <= 0 ||
      content.h <= 0) {
    return false;
  }
  const double dx = world.xmax - world.xmin;
  const double dy = world.ymax - world.ymin;
  *out_x = content.x + ((mx - world.xmin) / dx) * content.w;
  // World ymax is north; screen y grows downward.
  *out_y = content.y + ((world.ymax - my) / dy) * content.h;
  return true;
}

inline bool eagle_eye_px_to_world(const content::Extent2& world,
                                 double px,
                                 double py,
                                 const EagleEyeRectPx& content,
                                 double* out_mx,
                                 double* out_my) {
  if (!out_mx || !out_my || !eagle_eye_extent_nonempty(world) || content.w <= 0 ||
      content.h <= 0) {
    return false;
  }
  const double dx = world.xmax - world.xmin;
  const double dy = world.ymax - world.ymin;
  const double u = (px - content.x) / static_cast<double>(content.w);
  const double v = (py - content.y) / static_cast<double>(content.h);
  *out_mx = world.xmin + u * dx;
  *out_my = world.ymax - v * dy;
  return true;
}

inline EagleEyeRectPx eagle_eye_view_rect_px(const content::Extent2& world,
                                            const content::Extent2& view,
                                            const EagleEyeRectPx& content) {
  EagleEyeRectPx out{};
  if (!eagle_eye_extent_nonempty(world) || !eagle_eye_extent_nonempty(view) ||
      content.w <= 0 || content.h <= 0) {
    return out;
  }
  double x0 = 0;
  double y0 = 0;
  double x1 = 0;
  double y1 = 0;
  if (!eagle_eye_world_to_px(world, view.xmin, view.ymax, content, &x0, &y0) ||
      !eagle_eye_world_to_px(world, view.xmax, view.ymin, content, &x1, &y1)) {
    return out;
  }
  out.x = static_cast<int>(std::floor((std::min)(x0, x1)));
  out.y = static_cast<int>(std::floor((std::min)(y0, y1)));
  const int x_max = static_cast<int>(std::ceil((std::max)(x0, x1)));
  const int y_max = static_cast<int>(std::ceil((std::max)(y0, y1)));
  out.w = (std::max)(2, x_max - out.x);
  out.h = (std::max)(2, y_max - out.y);
  return out;
}

inline content::Extent2 eagle_eye_clamp_view_to_world(
    const content::Extent2& world,
    content::Extent2 view) {
  if (!eagle_eye_extent_nonempty(world) || !eagle_eye_extent_nonempty(view)) {
    return view;
  }
  const double vw = view.xmax - view.xmin;
  const double vh = view.ymax - view.ymin;
  const double ww = world.xmax - world.xmin;
  const double wh = world.ymax - world.ymin;
  if (vw >= ww) {
    view.xmin = world.xmin;
    view.xmax = world.xmax;
  } else {
    if (view.xmin < world.xmin) {
      view.xmin = world.xmin;
      view.xmax = world.xmin + vw;
    }
    if (view.xmax > world.xmax) {
      view.xmax = world.xmax;
      view.xmin = world.xmax - vw;
    }
  }
  if (vh >= wh) {
    view.ymin = world.ymin;
    view.ymax = world.ymax;
  } else {
    if (view.ymin < world.ymin) {
      view.ymin = world.ymin;
      view.ymax = world.ymin + vh;
    }
    if (view.ymax > world.ymax) {
      view.ymax = world.ymax;
      view.ymin = world.ymax - vh;
    }
  }
  return view;
}

// Center the current view size on a world point (click-to-navigate).
inline content::Extent2 eagle_eye_center_view_on(
    const content::Extent2& world,
    const content::Extent2& view,
    double mx,
    double my) {
  if (!eagle_eye_extent_nonempty(view)) {
    return view;
  }
  const double hw = (view.xmax - view.xmin) * 0.5;
  const double hh = (view.ymax - view.ymin) * 0.5;
  content::Extent2 next{mx - hw, my - hh, mx + hw, my + hh};
  return eagle_eye_clamp_view_to_world(world, next);
}

enum class EagleEyeHit {
  kNone,
  kFrame,
  kBox,
  kHandleNW,
  kHandleNE,
  kHandleSW,
  kHandleSE,
};

inline constexpr int kEagleEyeHandlePx = 7;

inline bool eagle_eye_pt_in_rect(int x, int y, const EagleEyeRectPx& r) {
  return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

inline EagleEyeHit eagle_eye_hit_test(int x,
                                      int y,
                                      const EagleEyeRectPx& frame,
                                      const EagleEyeRectPx& view_px) {
  if (!eagle_eye_pt_in_rect(x, y, frame)) {
    return EagleEyeHit::kNone;
  }
  const int hs = kEagleEyeHandlePx;
  auto near_handle = [&](int hx, int hy) {
    return x >= hx - hs && x <= hx + hs && y >= hy - hs && y <= hy + hs;
  };
  if (view_px.w > 0 && view_px.h > 0) {
    if (near_handle(view_px.x, view_px.y)) {
      return EagleEyeHit::kHandleNW;
    }
    if (near_handle(view_px.x + view_px.w, view_px.y)) {
      return EagleEyeHit::kHandleNE;
    }
    if (near_handle(view_px.x, view_px.y + view_px.h)) {
      return EagleEyeHit::kHandleSW;
    }
    if (near_handle(view_px.x + view_px.w, view_px.y + view_px.h)) {
      return EagleEyeHit::kHandleSE;
    }
    if (eagle_eye_pt_in_rect(x, y, view_px)) {
      return EagleEyeHit::kBox;
    }
  }
  return EagleEyeHit::kFrame;
}

// Resize view extent from a corner handle; keeps aspect = view aspect.
inline content::Extent2 eagle_eye_resize_view(
    const content::Extent2& world,
    const content::Extent2& start_view,
    EagleEyeHit handle,
    double mx,
    double my) {
  if (!eagle_eye_extent_nonempty(start_view) ||
      handle == EagleEyeHit::kNone || handle == EagleEyeHit::kFrame ||
      handle == EagleEyeHit::kBox) {
    return start_view;
  }
  content::Extent2 next = start_view;
  const double aspect =
      (start_view.xmax - start_view.xmin) /
      (std::max)(1e-12, start_view.ymax - start_view.ymin);
  switch (handle) {
    case EagleEyeHit::kHandleNW:
      next.xmin = mx;
      next.ymax = my;
      break;
    case EagleEyeHit::kHandleNE:
      next.xmax = mx;
      next.ymax = my;
      break;
    case EagleEyeHit::kHandleSW:
      next.xmin = mx;
      next.ymin = my;
      break;
    case EagleEyeHit::kHandleSE:
      next.xmax = mx;
      next.ymin = my;
      break;
    default:
      break;
  }
  if (next.xmax <= next.xmin || next.ymax <= next.ymin) {
    return start_view;
  }
  // Enforce aspect from the fixed opposite corner.
  double w = next.xmax - next.xmin;
  double h = next.ymax - next.ymin;
  if (w / h > aspect) {
    h = w / aspect;
  } else {
    w = h * aspect;
  }
  switch (handle) {
    case EagleEyeHit::kHandleNW:
      next.xmin = next.xmax - w;
      next.ymax = next.ymin + h;
      break;
    case EagleEyeHit::kHandleNE:
      next.xmax = next.xmin + w;
      next.ymax = next.ymin + h;
      break;
    case EagleEyeHit::kHandleSW:
      next.xmin = next.xmax - w;
      next.ymin = next.ymax - h;
      break;
    case EagleEyeHit::kHandleSE:
      next.xmax = next.xmin + w;
      next.ymin = next.ymax - h;
      break;
    default:
      break;
  }
  return eagle_eye_clamp_view_to_world(world, next);
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_EAGLE_EYE_GEOM_H_
