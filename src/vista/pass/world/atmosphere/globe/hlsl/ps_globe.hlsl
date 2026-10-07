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
    // Recycled R32 height / missing SRV samples as saturated red. Treat as
    // ocean so the globe never reads as a solid sun-disk.
    if (tex.r > 0.62 && tex.g < 0.28 && tex.b < 0.28)
    {
        tex = float4(0.05, 0.14, 0.32, 0.0);
    }
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
    // Soft wrap Lambert — clamp shade so DEM facets stay matte earth, not chrome.
    float wrap = ndl * 0.5 + 0.5;
    float shade = saturate(ambient + intensity * (wrap * wrap * 0.45 + ndl * 0.08));
    float3 lit = base * shade;
    // View-dependent atmosphere limb (Rayleigh-ish blue + warm sunset).
    float3 V = normalize(float3(eye_x, eye_y, eye_z) - input.world_pos);
    float ndv = saturate(dot(n, V));
    float limb = pow(1.0 - ndv, 3.0);
    float atmos = atmos_strength * limb;
    lit += atmos * float3(0.28, 0.48, 0.95);
    lit += atmos * atmos * 0.18 * float3(0.75, 0.55, 0.35);
    // Soft ocean gloss only — keep land matte (no Blinn on terrain).
    float water = 1.0 - land_w;
    float3 H = normalize(L + V);
    lit += water * intensity * 0.02 * pow(saturate(dot(n, H)), 48.0) *
           float3(0.80, 0.90, 1.0);
    return float4(saturate(lit), 1.0);
}
