// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/math/simd.h"

#if defined(SMT_RENDER_MATH_SIMD) && (defined(__AVX2__) || defined(__AVX2))
#include <immintrin.h>
#define SMT_MATH_HAVE_AVX2 1
#else
#define SMT_MATH_HAVE_AVX2 0
#endif

namespace render {

void normalize_batch(std::span<Vector3> points) {
#if SMT_MATH_HAVE_AVX2
  // AVX2 path still falls back per-element for clarity; widen later if needed.
  for (Vector3& p : points) {
    p.normalize();
  }
#else
  for (Vector3& p : points) {
    p.normalize();
  }
#endif
}

void transform_points_batch(const Matrix& m, std::span<Vector3> points) {
  for (Vector3& p : points) {
    const Vector4 out = m.transform_point(Vector4(p.x, p.y, p.z, 1.0f));
    p.set(out.x, out.y, out.z);
  }
}

}  // namespace render
