// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_GEOMETRY_INSETS_H_
#define UI_GFX_GEOMETRY_INSETS_H_

namespace ui {
namespace gfx {

// Integer padding on four sides (shell layout / horizon). Not a transform.
struct Insets {
  int top = 0;
  int left = 0;
  int bottom = 0;
  int right = 0;

  static Insets tlbr(int t, int l, int b, int r) {
    return Insets{t, l, b, r};
  }

  static Insets vh(int vertical, int horizontal) {
    return Insets{vertical, horizontal, vertical, horizontal};
  }

  int width() const { return left + right; }
  int height() const { return top + bottom; }
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_GEOMETRY_INSETS_H_
