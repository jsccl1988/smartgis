// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_SKY_HLSL_H_
#define EFFECT_ATMOSPHERE_SKY_HLSL_H_

namespace effect {
namespace atmosphere {

// Fullscreen NDC sky VS (same clip trick as fog). No CameraCB in VS.
inline constexpr char kVsSky[] = R"(
struct VSOut
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

VSOut main(float3 pos : POSITION)
{
    VSOut output;
    output.pos = float4(pos.xy, 0.999999, 1.0);
    output.ndc = pos.xy;
    return output;
}
)";

// Analytical sky PS: CameraCB b0 (view/proj) + SkyCB b1. Shading matches
// SkyPass::sample_sky_rgb. RH view-ray unproject uses CameraCB basis rows
// (industry fullscreen sky) — avoids transpose(view 3x3) diagonal seams.
inline constexpr char kPsSky[] = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

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
    float2 ndc : TEXCOORD0;
};

float4 main(PSIn input) : SV_TARGET
{
    // RH perspective: camera looks down -Z. Scale NDC by proj focal terms.
    float sx = max(abs(proj[0][0]), 1e-5);
    float sy = max(abs(proj[1][1]), 1e-5);
    float3 view_dir = float3(input.ndc.x / sx, input.ndc.y / sy, -1.0);
    // CameraCB view is column-major world-to-view (look_at). Rows are the
    // camera axes in world space: row0=right, row1=up, row2=-forward.
    // dir = R^T * view_dir without an explicit transpose (stable vs seams).
    float3 dir = normalize(view[0].xyz * view_dir.x +
                           view[1].xyz * view_dir.y +
                           view[2].xyz * view_dir.z);
    float3 sun = normalize(float3(sun_x, sun_y, sun_z));

    float elev = clamp(sun_y, -1.0, 1.0);
    float day = saturate(elev * 1.2 + 0.35);
    float3 sunset = float3(sunset_r, sunset_g, sunset_b);
    float3 horizon = float3(horizon_r, horizon_g, horizon_b);
    float3 zenith = float3(zenith_r, zenith_g, zenith_b);
    // Keep zenith clearly bluer than green (reject cyan wash).
    zenith.g = min(zenith.g, zenith.b * 0.55);
    zenith.r = min(zenith.r, zenith.b * 0.35);
    float3 ground = lerp(sunset, horizon, day);

    float elev_v = saturate(dir.y);
    float blend = pow(elev_v, 0.65);
    float3 rgb = lerp(ground, zenith, blend);
    float haze = saturate(1.0 - elev_v);
    rgb = saturate(rgb + ground * (haze * haze * 0.18));

    // Bruneton-lite (analytical, no LUT): Rayleigh zenith deepen, Mie-ish
    // warm horizon near sun azimuth, ozone-ish purple at low sun_y.
    float rayleigh = pow(elev_v, 0.55);
    rgb.r *= lerp(1.0, 0.80, rayleigh);
    rgb.g *= lerp(1.0, 0.94, rayleigh);
    rgb.b *= lerp(1.0, 1.14, rayleigh);
    float3 dir_h = normalize(float3(dir.x, 1e-3, dir.z));
    float3 sun_h = normalize(float3(sun.x, 1e-3, sun.z));
    float azi = saturate(dot(dir_h, sun_h));
    float mie_warm = pow(azi, 2.0) * haze * saturate(1.0 - abs(elev) * 0.55);
    rgb += float3(0.14, 0.055, 0.015) * mie_warm;
    float twilight = saturate(1.0 - abs(elev) * 3.5);
    float ozone = twilight * saturate(1.0 - elev_v * 1.15) * (0.30 + 0.70 * azi);
    rgb += float3(0.09, 0.02, 0.11) * ozone;
    rgb = saturate(rgb);

    // Compact sun disk (high exponent) + softer corona; scaled by sun_glow.
    float sun_dot = saturate(dot(dir, sun));
    float disk = pow(sun_dot, 256.0) * sun_glow * 1.55;
    float corona = pow(sun_dot, 12.0) * sun_glow * 0.42;
    rgb = saturate(rgb + disk * float3(1.0, 0.96, 0.88) +
                   corona * float3(1.0, 0.78, 0.48));
    return float4(rgb, 1.0);
}
)";

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_SKY_HLSL_H_
