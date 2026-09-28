// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_OCEAN_HLSL_H_
#define EFFECT_ATMOSPHERE_OCEAN_HLSL_H_

namespace effect {
namespace atmosphere {

// Verbatim HLSL copied from the FlyCube cache (kVsOcean).
inline constexpr char kVsOcean[] = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};
cbuffer OceanCB : register(b1)
{
    float4 deep;
    float4 shallow;
    float fresnel_bias;
    float fresnel_power;
    float height_scale;
    float cam_x;
    float cam_y;
    float cam_z;
    float disp_scale;
    float pad1;
};
Texture2D height_map : register(t0);
SamplerState linear_sampler : register(s0);

struct VSIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

VSOut main(VSIn input)
{
    float4 enc = height_map.SampleLevel(linear_sampler, input.uv, 0);
    float height = (enc.r - 0.5) * 2.0 * height_scale;
    float dx = (enc.g - 0.5) * 2.0 * disp_scale;
    float dz = (enc.b - 0.5) * 2.0 * disp_scale;
    float3 world = float3(input.pos.x + dx, height, input.pos.z + dz);
    VSOut output;
    output.world = world;
    output.uv = input.uv;
    float4 view_pos = mul(view, float4(world, 1.0));
    output.pos = mul(proj, view_pos);
    return output;
}
)";

// Verbatim HLSL copied from the FlyCube cache (kPsOcean).
inline constexpr char kPsOcean[] = R"(
cbuffer OceanCB : register(b1)
{
    float4 deep;
    float4 shallow;
    float fresnel_bias;
    float fresnel_power;
    float height_scale;
    float cam_x;
    float cam_y;
    float cam_z;
    float disp_scale;
    float pad1;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

float4 main(PSIn input) : SV_TARGET
{
    float3 dx = ddx(input.world);
    float3 dy = ddy(input.world);
    float3 n = normalize(cross(dx, dy));
    float3 cam = float3(cam_x, cam_y, cam_z);
    float3 V = normalize(cam - input.world);
    float ndotv = saturate(dot(n, V));
    float f = fresnel_bias + (1.0 - fresnel_bias) * pow(1.0 - ndotv, fresnel_power);
    float3 rgb = lerp(shallow.rgb, deep.rgb, f);
    return float4(rgb, lerp(shallow.a, deep.a, f));
}
)";

// Verbatim HLSL copied from the FlyCube cache (kCsOceanSpectrum).
inline constexpr char kCsOceanSpectrum[] = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

static const float kPi = 3.14159265358979323846;
static const float kG = 9.81;

float hash01(uint i, uint j, uint salt)
{
    uint h = i * 73856093u ^ j * 19349663u ^ salt * 83492791u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h & 0x00ffffffu) / float(0x01000000u);
}

float phillips(float kx, float ky)
{
    float k2 = kx * kx + ky * ky;
    if (k2 < 1.0e-8)
        return 0.0;
    float k_len = sqrt(k2);
    float L = max((wind_speed * wind_speed) / kG, 1.0e-3);
    float kwx = cos(wind_dir_rad);
    float kwy = sin(wind_dir_rad);
    float k_dot_w = max((kx * kwx + ky * kwy) / k_len, 0.0);
    float damp = exp(-1.0 / (k2 * L * L));
    float suppress = exp(-k2 * L * L * 0.001);
    return damp * suppress * (k_dot_w * k_dot_w) / (k2 * k2);
}

// Directional JONSWAP-lite on the k-grid (shape only; amp_scale sets Hs).
float jonswap(float kx, float ky)
{
    float k2 = kx * kx + ky * ky;
    if (k2 < 1.0e-8)
        return 0.0;
    float k_len = sqrt(k2);
    float omega = sqrt(kG * k_len);
    float U = max(wind_speed, 1.0);
    float omega_p = 0.877 * kG / U;
    float alpha = 0.0081;
    float sigma = (omega <= omega_p) ? 0.07 : 0.09;
    float om_r = omega_p / max(omega, 1.0e-4);
    float pm = alpha * kG * kG * pow(omega, -5.0) * exp(-1.25 * pow(om_r, 4.0));
    float r = (omega - omega_p) / max(sigma * omega_p, 1.0e-4);
    float peak = pow(max(gamma, 1.0), exp(-0.5 * r * r));
    float S_omega = pm * peak;
    float domega_dk = 0.5 * sqrt(kG / k_len);
    float S_k = S_omega * domega_dk / k_len;
    float kwx = cos(wind_dir_rad);
    float kwy = sin(wind_dir_rad);
    float k_dot_w = max((kx * kwx + ky * kwy) / k_len, 0.0);
    return S_k * (k_dot_w * k_dot_w);
}

RWTexture2D<float2> spectrum_uav : register(u0);
RWTexture2D<float2> spectrum_seed_uav : register(u1);

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    int ii = (int)id.x;
    int jj = (int)id.y;
    int half_n = (int)(size >> 1);
    if (ii >= half_n)
        ii -= (int)size;
    if (jj >= half_n)
        jj -= (int)size;
    if (ii == 0 && jj == 0)
    {
        spectrum_uav[id] = float2(0.0, 0.0);
        spectrum_seed_uav[id] = float2(0.0, 0.0);
        return;
    }
    float dk = 2.0 * kPi / max(patch_size, 1.0);
    float kx = float(ii) * dk;
    float ky = float(jj) * dk;
    float p = (spectrum_model == 0u) ? phillips(kx, ky) : jonswap(kx, ky);
    if (p <= 0.0)
    {
        spectrum_uav[id] = float2(0.0, 0.0);
        spectrum_seed_uav[id] = float2(0.0, 0.0);
        return;
    }
    float a = amp_scale * sqrt(p * 0.5);
    float phase0 = hash01(id.x, id.y, 1u) * 2.0 * kPi;
    float k_len = sqrt(kx * kx + ky * ky);
    float omega = sqrt(kG * k_len);
    float phase = phase0 + omega * time_sec;
    float2 h = float2(a * cos(phase), a * sin(phase));
    spectrum_uav[id] = h;
    spectrum_seed_uav[id] = h;
}
)";

// Verbatim HLSL copied from the FlyCube cache (kCsOceanBitReverse).
inline constexpr char kCsOceanBitReverse[] = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

Texture2D<float2> src_tex : register(t0);
RWTexture2D<float2> dst_tex : register(u0);

uint bit_reverse(uint x, uint bits)
{
    return reversebits(x) >> (32u - bits);
}

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    uint i = (direction == 0u) ? id.x : id.y;
    uint o = (direction == 0u) ? id.y : id.x;
    uint r = bit_reverse(i, log2_size);
    float2 v = (direction == 0u) ? src_tex[uint2(i, o)] : src_tex[uint2(o, i)];
    if (direction == 0u)
        dst_tex[uint2(r, o)] = v;
    else
        dst_tex[uint2(o, r)] = v;
}
)";

// Verbatim HLSL copied from the FlyCube cache (kCsOceanButterfly).
inline constexpr char kCsOceanButterfly[] = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

static const float kPi = 3.14159265358979323846;

Texture2D<float2> src_tex : register(t0);
RWTexture2D<float2> dst_tex : register(u0);

float2 mul_complex(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    // Inverse radix-2 Cooley-Tukey (after bit-reverse), ping-pong.
    if (id.x >= size || id.y >= size)
        return;
    uint i = (direction == 0u) ? id.x : id.y;
    uint o = (direction == 0u) ? id.y : id.x;
    uint len = 2u << stage;
    uint half = len >> 1;
    uint base = (i / len) * len;
    uint j = i % half;
    uint u = base + j;
    uint v = base + j + half;
    bool upper = (i % len) >= half;

    float2 a = (direction == 0u) ? src_tex[uint2(u, o)] : src_tex[uint2(o, u)];
    float2 b = (direction == 0u) ? src_tex[uint2(v, o)] : src_tex[uint2(o, v)];
    float ang = +2.0 * kPi * float(j) / float(len);
    float2 w = float2(cos(ang), sin(ang));
    float2 t = mul_complex(w, b);
    float2 result = upper ? (a - t) : (a + t);
    // Match CPU inverse scale at the final stage of each 1D transform.
    if (stage + 1u == log2_size)
        result *= (1.0 / float(size));
    if (direction == 0u)
        dst_tex[uint2(i, o)] = result;
    else
        dst_tex[uint2(o, i)] = result;
}
)";

// Verbatim HLSL copied from the FlyCube cache (kCsOceanDisplacementSpectrum).
inline constexpr char kCsOceanDisplacementSpectrum[] = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

static const float kPi = 3.14159265358979323846;

Texture2D<float2> src_tex : register(t0);
RWTexture2D<float2> dst_tex : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    int ii = (int)id.x;
    int jj = (int)id.y;
    int half_n = (int)(size >> 1);
    if (ii >= half_n)
        ii -= (int)size;
    if (jj >= half_n)
        jj -= (int)size;
    if (ii == 0 && jj == 0)
    {
        dst_tex[id] = float2(0.0, 0.0);
        return;
    }
    float dk = 2.0 * kPi / max(patch_size, 1.0);
    float kx = float(ii) * dk;
    float ky = float(jj) * dk;
    float k_len = sqrt(kx * kx + ky * ky);
    float k_hat = (direction == 0u) ? (kx / k_len) : (ky / k_len);
    float2 h = src_tex[id];
    // -i * (a+ib) = b - i a
    float2 disp = float2(h.y, -h.x) * (chop * k_hat);
    dst_tex[id] = disp;
}
)";

// Verbatim HLSL copied from the FlyCube cache (kCsOceanHeightEncode).
inline constexpr char kCsOceanHeightEncode[] = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

Texture2D<float2> src_tex : register(t0);
RWTexture2D<float4> height_uav : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    float v = src_tex[id].x;
    float scale = (encode_channel == 0u) ? height_scale : disp_scale;
    scale = max(scale, 1.0e-3);
    float enc = saturate(0.5 + 0.5 * (v / scale));
    float4 cur = height_uav[id];
    if (encode_channel == 0u)
        height_uav[id] = float4(enc, 0.5, 0.5, 1.0);
    else if (encode_channel == 1u)
    {
        cur.g = enc;
        height_uav[id] = cur;
    }
    else
    {
        cur.b = enc;
        height_uav[id] = cur;
    }
}
)";

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_OCEAN_HLSL_H_
