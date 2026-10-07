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

Texture2D<float4> src_tex : register(t0);
RWTexture2D<float4> dst_tex : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    int n = (int)size;
    int x = (int)id.x;
    int y = (int)id.y;
    float4 sum =
        src_tex[uint2((uint)((x - 2 + n) % n), id.y)] * (1.0 / 16.0) +
        src_tex[uint2((uint)((x - 1 + n) % n), id.y)] * (4.0 / 16.0) +
        src_tex[uint2(id.x, id.y)] * (6.0 / 16.0) +
        src_tex[uint2((uint)((x + 1) % n), id.y)] * (4.0 / 16.0) +
        src_tex[uint2((uint)((x + 2) % n), id.y)] * (1.0 / 16.0);
    dst_tex[id] = sum;
}
