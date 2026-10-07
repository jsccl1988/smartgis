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
