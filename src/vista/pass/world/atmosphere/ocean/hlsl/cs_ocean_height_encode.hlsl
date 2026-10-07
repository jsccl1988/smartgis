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
