// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MATH_SIMD_SIMD_H_
#define BASE_MATH_SIMD_SIMD_H_

#include "base/math/linear/affine2.h"
#include "base/math/linear/matrix.h"
#include "base/math/linear/vector.h"

#include <span>

namespace base {

// Batch float32 helpers. When BASE_MATH_SIMD is set and the TU is built
// with AVX2, an accelerated path is used; otherwise scalar loops.
void normalize_batch(std::span<Vector3> points);
void transform_points_batch(const Matrix& m, std::span<Vector3> points);

// Interleaved xy in / long xy out. Lengths must be even and equal.
// Scalar path matches transform_xy / leftover GDI LPToDP.
void transform_xy_batch(const LpToDp2& a, std::span<const float> xy_in,
                        std::span<long> xy_out);

}  // namespace base

namespace render {
using ::base::normalize_batch;
using ::base::transform_points_batch;
using ::base::transform_xy_batch;
}  // namespace render

#endif  // BASE_MATH_SIMD_SIMD_H_
