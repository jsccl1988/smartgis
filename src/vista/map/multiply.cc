// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/multiply.h"

#include <algorithm>

namespace vista {

void apply_multiply_coverage(std::span<std::uint8_t> rgba, float opacity) {
  if (rgba.empty() || (rgba.size() % 4) != 0) {
    return;
  }
  const float op = (std::max)(0.f, (std::min)(1.f, opacity));
  for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
    if (rgba[i + 3] < 160) {
      rgba[i] = 0;
      rgba[i + 1] = 0;
      rgba[i + 2] = 0;
      rgba[i + 3] = 0;
      continue;
    }
    const float r = static_cast<float>(rgba[i]) / 255.f;
    const float g = static_cast<float>(rgba[i + 1]) / 255.f;
    const float b = static_cast<float>(rgba[i + 2]) / 255.f;
    const float luma = 0.299f * r + 0.587f * g + 0.114f * b;
    const float m = (1.f - op) + op * luma;
    // Store the factor in every channel so a later dst *= src blend is
    // dst * ((1 - opacity) + opacity * luma), not a second luma multiply.
    const float clamped = (std::max)(0.f, (std::min)(1.f, m));
    const auto factor =
        static_cast<std::uint8_t>(clamped * 255.f + 0.5f);
    rgba[i] = factor;
    rgba[i + 1] = factor;
    rgba[i + 2] = factor;
    rgba[i + 3] = 255;
  }
}

}  // namespace vista
