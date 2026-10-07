// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Product face for C++26 data-parallel types (P1928). Today this is
// vir-simd's Parallelism TS polyfill (`vir::stdx`). When MSVC ships
// `#include <simd>`, switch the include / alias here in one place.
//
// Hot TUs that want AVX2-width `native_simd` must compile with
// `/arch:AVX2` (see //src/base/math:math_simd_avx2).

#ifndef BASE_SIMD_STDX_H_
#define BASE_SIMD_STDX_H_

#include <vir/simd.h>

namespace base {
namespace simd {

// Parallelism TS / C++26-candidate namespace used by product kernels.
namespace stdx = ::vir::stdx;

template <typename T>
using native = stdx::native_simd<T>;

template <typename T, int N>
using fixed = stdx::fixed_size_simd<T, N>;

}  // namespace simd
}  // namespace base

#endif  // BASE_SIMD_STDX_H_
