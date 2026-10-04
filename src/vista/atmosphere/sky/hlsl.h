// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_SKY_HLSL_H_
#define EFFECT_ATMOSPHERE_SKY_HLSL_H_

namespace vista {

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

// Analytical sky PS: SkyCB only. Screen-space Rayleigh (no CameraCB — DXC
// strips unused cbuffers and FlyCube GetBindKey("CameraCB") would fail).
// space_blend → deep-space starfield + faint Milky Way (globe splash path).
inline constexpr char kPsSky[] = R"(
cbuffer SkyCB : register(b1)
{
    float sun_x;
    float sun_y;
    float sun_z;
    float space_blend;
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

float hash21(float2 p)
{
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453123);
}

float3 sample_day_sky(float2 ndc)
{
    float elev_screen = saturate(ndc.y * 0.5 + 0.5);
    float3 horizon = float3(horizon_r, horizon_g, horizon_b);
    float3 zenith = float3(zenith_r, zenith_g, zenith_b);
    zenith.g = min(zenith.g, zenith.b * 0.55);
    zenith.r = min(zenith.r, zenith.b * 0.35);
    float3 ground = horizon;
    ground.r = min(ground.r, ground.b * 0.55);
    ground.g = min(ground.g, lerp(ground.g, ground.b, 0.30));
    float blend = pow(saturate(elev_screen * 1.25), 0.50);
    float3 rgb = lerp(ground, zenith, blend);
    float rayleigh = pow(elev_screen, 0.55);
    rgb.r *= lerp(1.0, 0.72, rayleigh);
    rgb.g *= lerp(1.0, 0.88, rayleigh);
    rgb.b *= lerp(1.0, 1.22, rayleigh);
    return saturate(rgb);
}

float3 sample_space(float2 ndc)
{
    float2 uv = ndc * 0.5 + 0.5;
    // Deep space with a cool nebula wash (not flat black — cinematic void).
    float3 rgb = float3(0.008, 0.010, 0.028);
    float band = exp(-pow((uv.y - 0.42) * 2.8, 2.0));
    float band2 = exp(-pow((uv.x - 0.55) * 1.6, 2.0));
    rgb += float3(0.045, 0.028, 0.090) * band * (0.55 + 0.45 * band2);
    rgb += float3(0.012, 0.018, 0.040) * (1.0 - uv.y) * 0.35;

    // Dense procedural stars (three layers — splash / universe read).
    float2 g1 = floor(uv * 260.0);
    float2 f1 = frac(uv * 260.0) - 0.5;
    float h1 = hash21(g1);
    float star1 = step(0.991, h1) *
                  saturate(1.0 - length(f1) * 3.6);
    float bright = lerp(0.55, 1.0, frac(h1 * 17.13));
    rgb += star1 * bright * float3(0.92, 0.94, 1.0);

    float2 g2 = floor(uv * 110.0 + 13.7);
    float2 f2 = frac(uv * 110.0 + 13.7) - 0.5;
    float h2 = hash21(g2 + 41.2);
    float star2 = step(0.978, h2) *
                  saturate(1.0 - length(f2) * 2.0);
    rgb += star2 * 0.45 * float3(0.75, 0.82, 1.0);

    float2 g3 = floor(uv * 48.0 + 7.3);
    float2 f3 = frac(uv * 48.0 + 7.3) - 0.5;
    float h3 = hash21(g3 + 91.7);
    float star3 = step(0.96, h3) *
                  saturate(1.0 - length(f3) * 1.4);
    rgb += star3 * 0.7 * float3(1.0, 0.92, 0.78);

    // Soft sun disk as a bright star when the sun is in-frame.
    float3 sun_dir = normalize(float3(sun_x, sun_y, sun_z));
    float2 sun_ndc = float2(sun_dir.x, sun_dir.y) * 0.85;
    float sun_d = length(ndc - sun_ndc);
    float disk = saturate(1.0 - sun_d * 28.0);
    float corona = exp(-sun_d * 14.0) * sun_glow;
    rgb += (disk * 1.4 + corona * 0.55) * float3(1.0, 0.96, 0.85);

    return saturate(rgb);
}

float4 main(PSIn input) : SV_TARGET
{
    float3 day = sample_day_sky(input.ndc);
    float3 space = sample_space(input.ndc);
    float w = saturate(space_blend);
    float3 rgb = lerp(day, space, w);
    return float4(rgb, 1.0);
}
)";

}  // namespace vista

#endif  // EFFECT_ATMOSPHERE_SKY_HLSL_H_
