// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_SIMD_H_
#define SMT_RENDER_MATH_SIMD_H_

#include "render/math/matrix.h"
#include "render/math/vector.h"

#include <span>

namespace render {

// Batch float32 helpers. When SMT_RENDER_MATH_SIMD is set and the TU is built
// with AVX2, an accelerated path is used; otherwise scalar loops.
void normalize_batch(std::span<Vector3> points);
void transform_points_batch(const Matrix& m, std::span<Vector3> points);

}  // namespace render

#endif  // SMT_RENDER_MATH_SIMD_H_
