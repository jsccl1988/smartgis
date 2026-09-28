// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_SKY_HLSL_H_
#define EFFECT_ATMOSPHERE_SKY_HLSL_H_

namespace effect {
namespace atmosphere {

// Dome VS: CameraCB b0, position in, world position out (orbit frame).
inline constexpr char kVsSky[] = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
};

VSOut main(float3 pos : POSITION)
{
    VSOut output;
    output.world = pos;
    float4 view_pos = mul(view, float4(pos, 1.0));
    output.pos = mul(proj, view_pos);
    return output;
}
)";

// Analytical sky PS: SkyCB b1. Matches SkyPass::sample_sky_rgb.
inline constexpr char kPsSky[] = R"(
cbuffer SkyCB : register(b1)
{
    float sun_x;
    float sun_y;
    float sun_z;
    float soft_pad;
    float zenith_r;
    float zenith_g;
    float zenith_b;
    float horizon_r;
    float horizon_g;
    float horizon_b;
    float sunset_r;
    float sunset_g;
    float sunset_b;
    float sun_glow;
    float cam_x;
    float cam_y;
    float cam_z;
    float pad;
    float pad1;
    float pad2;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
};

float4 main(PSIn input) : SV_TARGET
{
    float3 cam = float3(cam_x, cam_y, cam_z);
    float3 dir = normalize(input.world - cam);
    float3 sun = normalize(float3(sun_x, sun_y, sun_z));

    float elev = clamp(sun_y, -1.0, 1.0);
    float day = saturate(elev * 1.5 + 0.2);
    float3 sunset = float3(sunset_r, sunset_g, sunset_b);
    float3 horizon = float3(horizon_r, horizon_g, horizon_b);
    float3 zenith = float3(zenith_r, zenith_g, zenith_b);
    float3 ground = lerp(sunset, horizon, day);

    float vy = saturate(dir.y * 0.5 + 0.5);
    float blend = pow(vy, 0.55);
    float3 rgb = lerp(ground, zenith, blend);

    float sun_dot = saturate(dot(dir, sun));
    float glow = pow(sun_dot, 32.0) * sun_glow;
    rgb = saturate(rgb + float3(glow, glow * 0.9, glow * 0.7));
    return float4(rgb, 1.0);
}
)";

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_SKY_HLSL_H_
