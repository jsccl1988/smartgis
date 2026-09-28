// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_CLOUD_HLSL_H_
#define EFFECT_ATMOSPHERE_CLOUD_HLSL_H_

namespace effect {
namespace atmosphere {

// Verbatim HLSL copied from the FlyCube cache (kVsCloud).
inline constexpr char kVsCloud[] = R"(
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

// Verbatim HLSL copied from the FlyCube cache (kPsCloud).
inline constexpr char kPsCloud[] = R"(
cbuffer CloudCB : register(b1)
{
    float sun_x;
    float sun_y;
    float sun_z;
    float cover;
    float base_m;
    float top_m;
    float extinction;
    float steps;
    float cam_x;
    float cam_y;
    float cam_z;
    float pad0;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
};

float hash31(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float density_at(float3 p, float base_y, float top_y, float c)
{
    float lo = min(base_y, top_y);
    float hi = max(base_y, top_y);
    if (p.y < lo || p.y > hi || c <= 0.0)
        return 0.0;
    float t = (p.y - lo) / max(hi - lo, 1e-3);
    float falloff = 4.0 * t * (1.0 - t);
    float n = hash31(p * 0.35);
    return c * falloff * (0.35 + 0.65 * n);
}

float4 main(PSIn input) : SV_TARGET
{
    float3 cam = float3(cam_x, cam_y, cam_z);
    float3 dir = normalize(input.world - cam);
    float lo = min(base_m, top_m);
    float hi = max(base_m, top_m);
    // Slab intersection along Y.
    float t0, t1;
    if (abs(dir.y) < 1e-5)
    {
        if (cam.y < lo || cam.y > hi)
            discard;
        t0 = 0.0;
        t1 = 40.0;
    }
    else
    {
        float ta = (lo - cam.y) / dir.y;
        float tb = (hi - cam.y) / dir.y;
        t0 = max(min(ta, tb), 0.0);
        t1 = max(ta, tb);
        if (t1 <= t0)
            discard;
    }
    int nsteps = max(4, (int)steps);
    float ds = (t1 - t0) / (float)nsteps;
    float T = 1.0;
    float3 L = float3(0.0, 0.0, 0.0);
    float3 sun = normalize(float3(sun_x, sun_y, sun_z));
    for (int i = 0; i < 64; ++i)
    {
        if (i >= nsteps)
            break;
        float t = t0 + (float(i) + 0.5) * ds;
        float3 p = cam + dir * t;
        float dens = density_at(p, base_m, top_m, cover);
        if (dens <= 0.0)
            continue;
        float sigma = dens * max(extinction, 0.0);
        float step_T = exp(-sigma * ds);
        float shadow = exp(-density_at(p + sun * ds * 4.0, base_m, top_m, cover) *
                           max(extinction, 0.0) * ds * 4.0);
        L += T * (1.0 - step_T) * shadow * 0.08;
        T *= step_T;
        if (T < 0.02)
            break;
    }
    float alpha = saturate(1.0 - T);
    if (alpha < 0.01)
        discard;
    float3 rgb = saturate(L + float3(0.75, 0.78, 0.85) * alpha);
    return float4(rgb, alpha);
}
)";

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_CLOUD_HLSL_H_
