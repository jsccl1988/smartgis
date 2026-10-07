struct VSOut
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

VSOut main(float3 pos : POSITION)
{
    VSOut output;
    output.pos = float4(pos.xy, 0.999999, 1.0);
    output.ndc = pos.xy;
    return output;
}
