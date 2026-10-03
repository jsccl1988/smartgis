// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/frame/embed_fill.h"

namespace ui {
namespace views {
namespace detail {

void fill_map_embed_opaque(HDC hdc, const RECT& rc, bool scene3d) {
  if (!hdc || rc.right <= 0 || rc.bottom <= 0) {
    return;
  }
  RECT fill = {0, 0, rc.right, rc.bottom};
  HBRUSH brush =
      CreateSolidBrush(scene3d ? RGB(18, 32, 48) : RGB(170, 211, 223));
  FillRect(hdc, &fill, brush);
  DeleteObject(brush);
}

}  // namespace detail
}  // namespace views
}  // namespace ui
