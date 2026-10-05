// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_ATMOSPHERE_FOG_HLSL_H_
#define VISTA_PASS_ATMOSPHERE_FOG_HLSL_H_

namespace vista {

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

// Depth-aware aerial fog: CameraCB b0 (sky-style view-ray unproject) + FogCB
// b1 + optional depth_map t0. use_depth==0 skips the SRV (far-ray only).
inline constexpr char kPsFog[] = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

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
    float use_depth;
};

Texture2D depth_map : register(t0);
SamplerState linear_sampler : register(s0);

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

float4 main(PSIn input) : SV_TARGET
{
    float3 cam = float3(cam_x, cam_y, cam_z);
    float vis = max(visibility, 0.1);

    // RH perspective: same unproject as kPsSky (proj scale + view rotation).
    float sx = proj[0][0];
    float sy = proj[1][1];
    float3 view_dir = normalize(float3(input.ndc.x / max(sx, 1e-5),
                                       input.ndc.y / max(sy, 1e-5),
                                       -1.0));
    float3x3 R = (float3x3)view;
    float3 world_dir = normalize(mul(transpose(R), view_dir));

    float distance;
    float height_y;
    if (use_depth > 0.5)
    {
        // Screen UV from NDC (D3D: V grows downward).
        float2 uv = float2(input.ndc.x * 0.5 + 0.5,
                           0.5 - input.ndc.y * 0.5);
        float depth = depth_map.Sample(linear_sampler, uv).r;
        // Cleared / sky depth sits near 1.0. SkyPass already owns the
        // far-field color — do not apply far-ray haze (that washed the
        // 640x480 showcase to near-white and killed blue_sky gates).
        if (depth >= 0.999)
        {
            return float4(color_r, color_g, color_b, 0.0);
        }
        // RH proj: ndc.z in [0,1] (near..far). z_view = -B / (z + A).
        float A = proj[2][2];
        float B = proj[2][3];
        float z_view = -B / max(depth + A, 1e-5);
        // view_dir.z is negative (look -Z); keep it away from zero.
        float3 pos_view = view_dir * (z_view / min(view_dir.z, -1e-5));
        distance = length(pos_view);
        float3 world = cam + mul(transpose(R), pos_view);
        height_y = world.y;
    }
    else
    {
        // No depth SRV: far sample along the real view ray.
        distance = vis * 1.25;
        height_y = (cam + world_dir * distance).y;
    }

    float dens = max(density, 0.0);
    float dist_f = 1.0 - exp(-dens * (distance / vis));
    float above = max(0.0, height_y - base_height);
    float height_f = exp(-above * max(height_falloff, 0.0));
    float fog_factor =
        saturate(dist_f * height_f) * saturate(max_opacity);
    // color_* is the sky-tinted haze (filled by the host from FogDrawParams).
    return float4(color_r, color_g, color_b, fog_factor);
}
)";

}  // namespace vista

#endif  // VISTA_PASS_ATMOSPHERE_FOG_HLSL_H_
