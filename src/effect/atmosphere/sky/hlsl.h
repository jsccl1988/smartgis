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
// SkyPass::sample_sky_rgb. RH view-ray unproject matches FogPass (V^T).
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
    // Match FogPass / sky README: HLSL view[i] is column i of column-major
    // world-to-view. World ray = V^T * view_dir. Using view[i]*component
    // (V * view_dir) scrambles the dome into a pink mid-band wash.
    float3x3 R = (float3x3)view;
    float3 dir = mul(transpose(R), view_dir);
    float dir_len = length(dir);
    bool dir_ok = dir_len > 1e-5 && !any(isnan(dir)) && !any(isinf(dir));
    dir = dir_ok ? (dir / dir_len) : float3(0.0, 1.0, 0.0);
    float3 sun = normalize(float3(sun_x, sun_y, sun_z));

    float elev = clamp(sun_y, -1.0, 1.0);
    // Day floor bias: mid-elevation sun must not leave a magenta mid-band
    // (sunset-heavy ground) across the China orbit pitch.
    float day = saturate(elev * 1.35 + 0.55);
    float3 sunset = float3(sunset_r, sunset_g, sunset_b);
    float3 horizon = float3(horizon_r, horizon_g, horizon_b);
    float3 zenith = float3(zenith_r, zenith_g, zenith_b);
    // Keep zenith clearly bluer than green (reject cyan wash).
    zenith.g = min(zenith.g, zenith.b * 0.55);
    zenith.r = min(zenith.r, zenith.b * 0.35);
    // Cool the sunset leg so lerp(sunset, horizon, day) cannot go magenta.
    // Under daytime China sun, collapse sunset entirely into horizon.
    sunset = lerp(sunset, horizon, max(day, 0.70));
    sunset.r = min(sunset.r, sunset.b * 0.75);
    sunset.g = min(sunset.g, lerp(sunset.g, sunset.b, 0.35));
    float3 ground = lerp(sunset, horizon, day);

    // Screen-space elevation owns the Rayleigh dome (top = blue). Orbit pitch
    // looking down at China still leaves the top third blue for the gate;
    // view-ray elev alone washed mid-frame to pale teal under a low sun.
    float elev_screen = saturate(input.ndc.y * 0.5 + 0.5);
    float elev_view = saturate(dir.y);
    float elev_v = dir_ok ? max(elev_screen, elev_view * 0.25) : elev_screen;
    // Strong zenith bias: mid-frame must stay Rayleigh blue (pink_frac_top
    // gate). Soften sunset/horizon mix so the upper third is not magenta.
    float blend = pow(saturate(elev_v * 1.45), 0.38);
    float3 rgb = lerp(ground, zenith, blend);
    float haze = saturate(1.0 - elev_v);
    rgb = saturate(rgb + ground * (haze * haze * 0.02));

    // Bruneton-lite (analytical, no LUT): Rayleigh zenith deepen, Mie-ish
    // warm horizon near sun azimuth — keep red contribution tiny.
    float rayleigh = pow(elev_v, 0.55);
    rgb.r *= lerp(1.0, 0.72, rayleigh);
    rgb.g *= lerp(1.0, 0.88, rayleigh);
    rgb.b *= lerp(1.0, 1.22, rayleigh);
    float3 dir_h = normalize(float3(dir.x, 1e-3, dir.z));
    float3 sun_h = normalize(float3(sun.x, 1e-3, sun.z));
    float azi = dir_ok ? saturate(dot(dir_h, sun_h)) : 0.0;
    float mie_warm = pow(azi, 2.0) * haze * saturate(1.0 - abs(elev) * 0.55);
    // Keep Mie off the upper/mid dome so zenith stays gate-blue.
    mie_warm *= saturate(1.0 - elev_screen * 1.8);
    mie_warm *= 0.35;
    rgb += float3(0.03, 0.02, 0.015) * mie_warm;
    float twilight = saturate(1.0 - abs(elev) * 3.5);
    float ozone = twilight * saturate(1.0 - elev_v * 1.35) * (0.15 + 0.35 * azi);
    ozone *= saturate(1.0 - elev_screen * 1.4);
    ozone *= 0.25;
    // Prefer cool ozone (blue) over magenta.
    rgb += float3(0.015, 0.01, 0.04) * ozone;
    rgb = saturate(rgb);

    // Compact sun disk only on trustworthy rays AND near the horizon band —
    // a soft corona over mid-elev washed atmosphere.full to pale teal.
    if (dir_ok && elev_screen < 0.55) {
      float sun_dot = saturate(dot(dir, sun));
      float disk = pow(sun_dot, 320.0) * sun_glow * 1.2;
      float corona = pow(sun_dot, 64.0) * sun_glow * 0.12;
      rgb = saturate(rgb + disk * float3(1.0, 0.96, 0.88) +
                     corona * float3(1.0, 0.78, 0.48));
    }
    return float4(rgb, 1.0);
}
)";

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_SKY_HLSL_H_
