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
    float sun_x;
    float sun_y;
    float sun_z;
    float shininess;
    float pad;
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
    // Far lip of the China patch used to displace into the sky dome. Keep
    // chop near the camera; flatten toward the horizon.
    float dist_xz = length(float2(input.pos.x - cam_x, input.pos.z - cam_z));
    float atten = saturate(1.15 - dist_xz * 0.28);
    height *= atten;
    dx *= atten;
    dz *= atten;
    float3 world = float3(input.pos.x + dx, input.pos.y + height, input.pos.z + dz);
    VSOut output;
    output.world = world;
    output.uv = input.uv;
    float4 view_pos = mul(view, float4(world, 1.0));
    output.pos = mul(proj, view_pos);
    return output;
}
