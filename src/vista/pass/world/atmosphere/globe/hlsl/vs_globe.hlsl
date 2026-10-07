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
    // RH CameraCB: look down -Z. Ground-hug DEM tris can sit inside near and
    // rasterize as solid clear cyan slabs; clamp view depth just beyond near.
    const float kMinViewZ = -0.12;
    if (eye.z > kMinViewZ)
    {
        eye.z = kMinViewZ;
    }
    o.pos = mul(proj, eye);
    o.uv = input.uv;
    o.world_pos = input.pos;
    // Mesh carries DEM surface normals (kPositionNormalUv) for PS Lambert.
    o.world_n = input.nrm;
    return o;
}
