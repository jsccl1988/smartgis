// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_AFFINE2_H_
#define SMT_RENDER_MATH_AFFINE2_H_

namespace render {

// Logical→device map matching leftover GDI LPToDP:
//   X = LONG(vox + (x - wox) * scale + 0.5)
//   Y = LONG(voy + (y - woy) * scale + 0.5)
//   Y = LONG(view_h - Y) when flip_y
struct LpToDp2 {
  float wox = 0.f;
  float woy = 0.f;
  float vox = 0.f;
  float voy = 0.f;
  float scale = 1.f;
  float view_h = 0.f;
  bool flip_y = true;
};

inline void transform_xy(const LpToDp2& a, float x, float y, long* ox,
                         long* oy) {
  long X = static_cast<long>(a.vox + (x - a.wox) * a.scale + 0.5f);
  long Y = static_cast<long>(a.voy + (y - a.woy) * a.scale + 0.5f);
  if (a.flip_y) {
    Y = static_cast<long>(a.view_h - Y);
  }
  if (ox) {
    *ox = X;
  }
  if (oy) {
    *oy = Y;
  }
}

}  // namespace render

#endif  // SMT_RENDER_MATH_AFFINE2_H_
