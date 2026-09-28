// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_FOG_HLSL_H_
#define EFFECT_ATMOSPHERE_FOG_HLSL_H_

namespace effect {
namespace atmosphere {

// Fullscreen NDC fog: clip-space pass-through (no CameraCB in VS). Far Z so
// optional depth tests still treat haze as behind near geometry when enabled.
inline constexpr char kVsFog[] = R"(
struct VSOut
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

VSOut main(float3 pos : POSITION)
{
    VSOut output;
    // pos.xy already in clip/NDC; push to far plane.
    output.pos = float4(pos.xy, 0.999999, 1.0);
    output.ndc = pos.xy;
    return output;
}
)";

// View-ray distance × height falloff from NDC (Approach B without depth RT).
// cam_* is the orbit eye; ndc approximates a view ray into the China frame.
inline constexpr char kPsFog[] = R"(
cbuffer FogCB : register(b1)
{
    float density;
    float height_falloff;
    float base_height;
    float visibility;
    float color_r;
    float color_g;
    float color_b;
    float max_opacity;
    float cam_x;
    float cam_y;
    float cam_z;
    float pad0;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

float4 main(PSIn input) : SV_TARGET
{
    float3 cam = float3(cam_x, cam_y, cam_z);
    // Approximate a far world sample along the view ray from NDC.
    float vis = max(visibility, 0.1);
    float3 dir = normalize(float3(input.ndc.x, input.ndc.y, 1.0));
    float3 world = cam + dir * (vis * 1.25);
    float distance = length(world - cam);
    float height_y = world.y;
    float dens = max(density, 0.0);
    float dist_f = 1.0 - exp(-dens * (distance / vis));
    float above = max(0.0, height_y - base_height);
    float height_f = exp(-above * max(height_falloff, 0.0));
    // Soften edges so the full-frame wash does not crush terrain contrast.
    float edge = saturate(1.0 - length(input.ndc) * 0.35);
    float fog_factor =
        saturate(dist_f * height_f) * saturate(max_opacity) * edge;
    return float4(color_r, color_g, color_b, fog_factor);
}
)";

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_FOG_HLSL_H_
