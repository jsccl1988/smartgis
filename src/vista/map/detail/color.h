// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Straight 0xAARRGGBB unpack shared by placement, the glyph atlas, and clear.

#ifndef EFFECT_MAP_DETAIL_COLOR_H_
#define EFFECT_MAP_DETAIL_COLOR_H_

#include <algorithm>
#include <cstdint>

namespace vista {
namespace detail {

inline float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(hi, v));
}

// 0xAARRGGBB, matching gis::style::ResolvedPaint. Straight RGB; alpha is the
// packed A times opacity.
inline void unpack_rgba(uint32_t rgba, float opacity, float* r, float* g,
                        float* b, float* a) {
  const float inv = 1.f / 255.f;
  if (r) {
    *r = static_cast<float>((rgba >> 16) & 0xffu) * inv;
  }
  if (g) {
    *g = static_cast<float>((rgba >> 8) & 0xffu) * inv;
  }
  if (b) {
    *b = static_cast<float>(rgba & 0xffu) * inv;
  }
  if (a) {
    *a = static_cast<float>((rgba >> 24) & 0xffu) * inv *
         clampf(opacity, 0.f, 1.f);
  }
}

}  // namespace detail
}  // namespace vista

#endif  // EFFECT_MAP_DETAIL_COLOR_H_
