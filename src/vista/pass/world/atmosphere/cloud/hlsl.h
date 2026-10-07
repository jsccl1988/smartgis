// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_CLOUD_HLSL_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_CLOUD_HLSL_H_

// Shader source of truth: sibling *.hlsl files. GN embed_hlsl generates
// *.hlsl.inc raw-string literals under $root_gen_dir (see embed_hlsl.gni).

namespace vista {

// Verbatim HLSL copied from the FlyCube cache (kVsCloud).
inline constexpr char kVsCloud[] =
#include "vista/pass/world/atmosphere/cloud/vs_cloud.hlsl.inc"
    ;

// Verbatim HLSL (kPsCloud). Powder + silver-lining brighten scatter; Beer T stays.
// density_cell > 0 snaps sample positions (half-res quality proxy until RHI RTs).
inline constexpr char kPsCloud[] =
#include "vista/pass/world/atmosphere/cloud/ps_cloud.hlsl.inc"
    ;

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_CLOUD_HLSL_H_
