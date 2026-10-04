// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_GLOBE_HLSL_H_
#define EFFECT_ATMOSPHERE_GLOBE_HLSL_H_

namespace vista {

// Lit textured earth sphere. POSITION+NORMAL+UV; CameraCB b0; GlobeCB b1; t0.
inline constexpr char kVsGlobe[] = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 world_pos : TEXCOORD1;
    float3 world_n : TEXCOORD2;
};

VSOut main(VSIn input)
{
    VSOut o;
    float4 wp = float4(input.pos, 1.0);
    float4 eye = mul(view, wp);
    o.pos = mul(proj, eye);
    o.uv = input.uv;
    o.world_pos = input.pos;
    // Mesh carries DEM surface normals (kPositionNormalUv) for PS Lambert.
    o.world_n = input.nrm;
    return o;
}
)";

inline constexpr char kPsGlobe[] = R"(
cbuffer GlobeCB : register(b1)
{
    float sun_x;
    float sun_y;
    float sun_z;
    float ambient;
    float intensity;
    float ocean_r;
    float ocean_g;
    float ocean_b;
    float eye_x;
    float eye_y;
    float eye_z;
    float atmos_strength;
};

Texture2D albedo_tex : register(t0);
SamplerState linear_sampler : register(s0);

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 world_pos : TEXCOORD1;
    float3 world_n : TEXCOORD2;
};

float4 main(PSIn input) : SV_TARGET
{
    float4 tex = albedo_tex.Sample(linear_sampler, input.uv);
    // Alpha < 0.5 marks ocean cells in the equirect bake — tint deep blue.
    // Soft alpha from edge fade blends land into the ocean tint.
    float3 ocean = float3(ocean_r, ocean_g, ocean_b);
    float land_w = saturate(tex.a);
    float3 base = lerp(ocean, tex.rgb, land_w);
    if (tex.a < 0.02)
    {
        base = ocean;
    }
    float3 n = normalize(input.world_n);
    float3 L = normalize(float3(sun_x, sun_y, sun_z));
    float ndl = saturate(dot(n, L));
    // Soft Lambert terminator — night side stays readable for splash frames.
    float shade = ambient + intensity * (ndl * 0.82 + ndl * ndl * 0.18);
    float3 lit = base * shade;
    // View-dependent atmosphere limb (Rayleigh-ish blue + warm sunset).
    float3 V = normalize(float3(eye_x, eye_y, eye_z) - input.world_pos);
    float ndv = saturate(dot(n, V));
    float limb = pow(1.0 - ndv, 3.0);
    float atmos = atmos_strength * limb;
    lit += atmos * float3(0.28, 0.48, 0.95);
    lit += atmos * atmos * 0.35 * float3(0.75, 0.55, 0.35);
    return float4(saturate(lit), 1.0);
}
)";

// Transparent satellite cloud shell (POSITION+UV). Alpha from cover texture.
inline constexpr char kVsSatCloud[] = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

struct VSIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut main(VSIn input)
{
    VSOut o;
    float4 eye = mul(view, float4(input.pos, 1.0));
    o.pos = mul(proj, eye);
    o.uv = input.uv;
    return o;
}
)";

inline constexpr char kPsSatCloud[] = R"(
cbuffer SatCloudCB : register(b1)
{
    float opacity;
    float soft_edge;
    float time_sec;
    float pad;
};

Texture2D cover_tex : register(t0);
SamplerState linear_sampler : register(s0);

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSIn input) : SV_TARGET
{
    // Mild eastward scroll so static cover still reads as weather motion.
    float2 uv = input.uv + float2(frac(time_sec * 0.0025), 0.0);
    float4 c = cover_tex.Sample(linear_sampler, uv);
    float cover = saturate(max(c.a, max(c.r, max(c.g, c.b))));
    float a = saturate((cover - soft_edge) / max(1.0 - soft_edge, 1e-3)) * opacity;
    float3 rgb = lerp(float3(0.85, 0.88, 0.92), float3(1.0, 1.0, 1.0), cover);
    return float4(rgb, a);
}
)";

}  // namespace vista

#endif  // EFFECT_ATMOSPHERE_GLOBE_HLSL_H_
