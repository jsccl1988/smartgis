// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MATH_LINEAR_AFFINE2_H_
#define BASE_MATH_LINEAR_AFFINE2_H_

namespace base {

// Logical→device map matching leftover GDI LPToDP:
//   X = LONG(vox + (x - wox) * scale + 0.5)
//   Y = LONG(voy + (y - woy) * scale + 0.5)
//   Y = LONG(view_h - Y) when flip_y
// Eigen-backed Vector/Matrix live elsewhere; this stays a 2D LP↔DP special case
// (do not force through 4×4 Matrix).
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

// Inverse of transform_xy (matches leftover DPToLP: unflip then divide).
inline void inverse_xy(const LpToDp2& a, long X, long Y, float* ox, float* oy) {
  long Yu = Y;
  if (a.flip_y) {
    Yu = static_cast<long>(a.view_h - Y);
  }
  if (ox) {
    *ox = (static_cast<float>(X) - a.vox) / a.scale + a.wox;
  }
  if (oy) {
    *oy = (static_cast<float>(Yu) - a.voy) / a.scale + a.woy;
  }
}

}  // namespace base

namespace render {
using ::base::LpToDp2;
using ::base::transform_xy;
using ::base::inverse_xy;
}  // namespace render

#endif  // BASE_MATH_LINEAR_AFFINE2_H_
