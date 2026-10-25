// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_EAGLE_EYE_PAINT_H_
#define UI_VIEWS_MAP_EAGLE_EYE_PAINT_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/public/types.h"

namespace ui {
namespace views {
namespace detail {

// Offscreen GDI compose of overview chrome + world fill + view box / handles.
void eagle_eye_paint(HDC hdc,
                     int frame_w,
                     int frame_h,
                     const content::Extent2& world,
                     const content::Extent2& view);

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_EAGLE_EYE_PAINT_H_
