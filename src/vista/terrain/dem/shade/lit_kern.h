// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// DEM shade / map-drape lit kernels. Scalar dispatch lives in lit_dispatch.cc;
// AVX2 is a separate TU (lit_avx2.cc) compiled with /arch:AVX2.

#ifndef VISTA_TERRAIN_DEM_SHADE_LIT_KERN_H_
#define VISTA_TERRAIN_DEM_SHADE_LIT_KERN_H_

#include <cstddef>
#include <cstdint>

namespace vista {
namespace detail {

// Multiply packed RGBA8 by (bias + scale * clamp(shade, 0.08, 1)).
// |shade[i]| < 0 skips the pixel (ocean / nodata already authored).
// |n| is pixel count; |rgba| length is n*4.
void apply_rgb_lit_mul(uint8_t* rgba, const float* shade, size_t n, float bias,
                       float scale);

void apply_rgb_lit_mul_scalar(uint8_t* rgba, const float* shade, size_t n,
                              float bias, float scale);

// Processes complete groups of 8 pixels. Caller owns the tail.
void apply_rgb_lit_mul_avx2(uint8_t* rgba, const float* shade, size_t n,
                            float bias, float scale);

// Lambert lerp into RGBA8 from a shade grid. When |heights| is non-null,
// cells with height <= 1 stay A=0 (ocean). Contrast matches encode_shade.
void pack_lambert_from_shade(const float* shade, const float* heights,
                             uint8_t* rgba, size_t n, float sr, float sg,
                             float sb, float hr, float hg, float hb,
                             bool contrast);

void pack_lambert_from_shade_scalar(const float* shade, const float* heights,
                                    uint8_t* rgba, size_t n, float sr, float sg,
                                    float sb, float hr, float hg, float hb,
                                    bool contrast);

void pack_lambert_from_shade_avx2(const float* shade, const float* heights,
                                  uint8_t* rgba, size_t n, float sr, float sg,
                                  float sb, float hr, float hg, float hb,
                                  bool contrast);

bool dem_lit_avx2_compiled();
bool dem_lit_avx2_runtime();

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_SHADE_LIT_KERN_H_
