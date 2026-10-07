// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Coverage kernels. The public entry is apply_multiply_coverage.
// Benchmarks call these directly so scalar and AVX2 are timed apart.

#ifndef VISTA_COMPONENT_MAP_SHADE_MULTIPLY_KERN_H_
#define VISTA_COMPONENT_MAP_SHADE_MULTIPLY_KERN_H_

#include <cstddef>
#include <cstdint>

namespace vista {
namespace detail {

void apply_multiply_coverage_scalar(std::uint8_t* rgba, size_t pixels,
                                    float opacity);

// Processes complete groups of 8 pixels. The caller owns the tail.
void apply_multiply_coverage_avx2(std::uint8_t* rgba, size_t pixels,
                                  float opacity);

// True when this build contains the AVX2 TU.
bool multiply_avx2_compiled();

// True when the AVX2 TU is present and the CPU/OS can run YMM code.
bool multiply_avx2_runtime();

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_SHADE_MULTIPLY_KERN_H_
