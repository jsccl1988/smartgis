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
