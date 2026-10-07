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
