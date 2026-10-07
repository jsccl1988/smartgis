// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_FOG_HLSL_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_FOG_HLSL_H_

// Shader source of truth: sibling *.hlsl files. GN embed_hlsl generates
// *.hlsl.inc raw-string literals under $root_gen_dir (see embed_hlsl.gni).

namespace vista {

// Fullscreen NDC fog: clip-space pass-through (no CameraCB in VS). Far Z so
// optional depth tests still treat haze as behind near geometry when enabled.
inline constexpr char kVsFog[] =
#include "vista/pass/world/atmosphere/fog/vs_fog.hlsl.inc"
    ;

// Depth-aware aerial fog: CameraCB b0 (sky-style view-ray unproject) + FogCB
// b1 + optional depth_map t0. use_depth==0 skips the SRV (far-ray only).
inline constexpr char kPsFog[] =
#include "vista/pass/world/atmosphere/fog/ps_fog.hlsl.inc"
    ;

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_FOG_HLSL_H_
