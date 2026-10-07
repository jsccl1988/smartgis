// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_SKY_HLSL_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_SKY_HLSL_H_

// Shader source of truth: sky/hlsl/*.hlsl. GN embed_hlsl generates
// *.hlsl.inc raw-string literals under $root_gen_dir (see embed_hlsl.gni).

namespace vista {

// Fullscreen NDC sky VS (same clip trick as fog). No CameraCB in VS.
inline constexpr char kVsSky[] =
#include "vista/pass/world/atmosphere/sky/hlsl/vs_sky.hlsl.inc"
    ;

// Analytical sky PS: SkyCB only. Screen-space Rayleigh (no CameraCB — DXC
// strips unused cbuffers and FlyCube GetBindKey("CameraCB") would fail).
// space_blend → deep-space starfield + faint Milky Way (globe splash path).
inline constexpr char kPsSky[] =
#include "vista/pass/world/atmosphere/sky/hlsl/ps_sky.hlsl.inc"
    ;

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_SKY_HLSL_H_
