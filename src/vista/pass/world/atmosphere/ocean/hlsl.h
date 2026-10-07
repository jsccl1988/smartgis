// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_OCEAN_HLSL_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_OCEAN_HLSL_H_

// Shader source of truth: sibling *.hlsl files. GN embed_hlsl generates
// *.hlsl.inc raw-string literals under $root_gen_dir (see embed_hlsl.gni).

namespace vista {

// Ocean graphics VS: sample encoded height/disp and displace the patch mesh.
inline constexpr char kVsOcean[] =
#include "vista/pass/world/atmosphere/ocean/vs_ocean.hlsl.inc"
    ;

// Ocean graphics PS: central-difference normals from height_map, Fresnel
// deep/shallow, Blinn-Phong sun specular, weak high-slope foam.
inline constexpr char kPsOcean[] =
#include "vista/pass/world/atmosphere/ocean/ps_ocean.hlsl.inc"
    ;

// Verbatim HLSL copied from the FlyCube cache (kCsOceanSpectrum).
inline constexpr char kCsOceanSpectrum[] =
#include "vista/pass/world/atmosphere/ocean/cs_ocean_spectrum.hlsl.inc"
    ;

// Verbatim HLSL copied from the FlyCube cache (kCsOceanBitReverse).
inline constexpr char kCsOceanBitReverse[] =
#include "vista/pass/world/atmosphere/ocean/cs_ocean_bit_reverse.hlsl.inc"
    ;

// Verbatim HLSL copied from the FlyCube cache (kCsOceanButterfly).
inline constexpr char kCsOceanButterfly[] =
#include "vista/pass/world/atmosphere/ocean/cs_ocean_butterfly.hlsl.inc"
    ;

// Verbatim HLSL copied from the FlyCube cache (kCsOceanDisplacementSpectrum).
inline constexpr char kCsOceanDisplacementSpectrum[] =
#include "vista/pass/world/atmosphere/ocean/cs_ocean_displacement_spectrum.hlsl.inc"
    ;

// Verbatim HLSL copied from the FlyCube cache (kCsOceanHeightEncode).
inline constexpr char kCsOceanHeightEncode[] =
#include "vista/pass/world/atmosphere/ocean/cs_ocean_height_encode.hlsl.inc"
    ;

// Separable 5-tap Gaussian blur (horizontal) on the encoded RGBA height map.
// Weights [1,4,6,4,1]/16 with periodic wrap matching the FFT patch.
inline constexpr char kCsOceanGaussianH[] =
#include "vista/pass/world/atmosphere/ocean/cs_ocean_gaussian_h.hlsl.inc"
    ;

// Separable 5-tap Gaussian blur (vertical) on the encoded RGBA height map.
inline constexpr char kCsOceanGaussianV[] =
#include "vista/pass/world/atmosphere/ocean/cs_ocean_gaussian_v.hlsl.inc"
    ;

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_OCEAN_HLSL_H_
