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
