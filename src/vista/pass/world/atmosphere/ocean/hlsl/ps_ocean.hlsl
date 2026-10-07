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
    float sun_x;
    float sun_y;
    float sun_z;
    float shininess;
    float pad;
};
Texture2D height_map : register(t0);
SamplerState linear_sampler : register(s0);

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

float3 decode_disp(float4 enc)
{
    return float3((enc.g - 0.5) * 2.0 * disp_scale,
                  (enc.r - 0.5) * 2.0 * height_scale,
                  (enc.b - 0.5) * 2.0 * disp_scale);
}

float4 main(PSIn input) : SV_TARGET
{
    float w = 0.0;
    float h = 0.0;
    height_map.GetDimensions(w, h);
    float2 texel = float2(1.0 / max(w, 1.0), 1.0 / max(h, 1.0));

    float3 pl = decode_disp(height_map.Sample(linear_sampler, input.uv - float2(texel.x, 0.0)));
    float3 pr = decode_disp(height_map.Sample(linear_sampler, input.uv + float2(texel.x, 0.0)));
    float3 pd = decode_disp(height_map.Sample(linear_sampler, input.uv - float2(0.0, texel.y)));
    float3 pu = decode_disp(height_map.Sample(linear_sampler, input.uv + float2(0.0, texel.y)));

    // Base-grid step keeps flat water upward; displacement deltas tilt the normal.
    float3 dPdu = (pr - pl) + float3(2.0, 0.0, 0.0);
    float3 dPdv = (pu - pd) + float3(0.0, 0.0, 2.0);
    float3 n = normalize(cross(dPdv, dPdu));
    if (n.y < 0.0)
        n = -n;

    float3 cam = float3(cam_x, cam_y, cam_z);
    float3 V = normalize(cam - input.world);
    float ndotv = saturate(dot(n, V));
    // Grazing outer ring replaces SkyPass with a cyan sheet on the full
    // China orbit. Near water (coast) stays; the horizon belongs to the sky.
    float dist_xz = length(float2(input.world.x - cam_x, input.world.z - cam_z));
    if (ndotv < 0.16 && dist_xz > 1.25)
        discard;
    float f = fresnel_bias + (1.0 - fresnel_bias) * pow(1.0 - ndotv, fresnel_power);
    float3 rgb = lerp(shallow.rgb, deep.rgb, f);
    // Grazing water should pick up sky blue, not a blown cyan albedo.
    float3 sky_refl = float3(0.30, 0.48, 0.78);
    rgb = lerp(rgb, sky_refl, saturate(f * 0.45));

    float3 L = normalize(float3(sun_x, sun_y, sun_z));
    float3 H = normalize(L + V);
    // Tiny sun glint only — strong Spec blew a cyan flare over the orbit
    // patch and replaced the sky far-field in 640x480 captures.
    float spec = pow(saturate(dot(n, H)), max(shininess, 96.0));
    rgb += spec * float3(0.10, 0.12, 0.14);

    float slope = length(float2(pr.y - pl.y, pu.y - pd.y));
    float foam = saturate(slope * 1.8 - 0.28) * 0.08;
    rgb = lerp(rgb, float3(0.55, 0.68, 0.78), foam);
    rgb = saturate(rgb);

    return float4(rgb, lerp(shallow.a, deep.a, f));
}
