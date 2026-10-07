// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_GLOBE_HLSL_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_GLOBE_HLSL_H_

// Shader source of truth: globe/hlsl/*.hlsl. GN embed_hlsl generates
// *.hlsl.inc raw-string literals under $root_gen_dir (see embed_hlsl.gni).

namespace vista {

// Lit textured earth sphere. POSITION+NORMAL+UV; CameraCB b0 (VS);
// GlobeCB b1 (PS lit); albedo t0.
inline constexpr char kVsGlobe[] =
#include "vista/pass/world/atmosphere/globe/hlsl/vs_globe.hlsl.inc"
    ;

inline constexpr char kPsGlobe[] =
#include "vista/pass/world/atmosphere/globe/hlsl/ps_globe.hlsl.inc"
    ;

// Transparent satellite cloud shell (POSITION+UV). Alpha from cover texture.
inline constexpr char kVsSatCloud[] =
#include "vista/pass/world/atmosphere/globe/hlsl/vs_sat_cloud.hlsl.inc"
    ;

inline constexpr char kPsSatCloud[] =
#include "vista/pass/world/atmosphere/globe/hlsl/ps_sat_cloud.hlsl.inc"
    ;

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_GLOBE_HLSL_H_
