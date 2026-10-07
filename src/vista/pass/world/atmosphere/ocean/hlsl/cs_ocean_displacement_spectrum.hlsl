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

static const float kPi = 3.14159265358979323846;

Texture2D<float2> src_tex : register(t0);
RWTexture2D<float2> dst_tex : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    int ii = (int)id.x;
    int jj = (int)id.y;
    int half_n = (int)(size >> 1);
    if (ii >= half_n)
        ii -= (int)size;
    if (jj >= half_n)
        jj -= (int)size;
    if (ii == 0 && jj == 0)
    {
        dst_tex[id] = float2(0.0, 0.0);
        return;
    }
    float dk = 2.0 * kPi / max(patch_size, 1.0);
    float kx = float(ii) * dk;
    float ky = float(jj) * dk;
    float k_len = sqrt(kx * kx + ky * ky);
    float k_hat = (direction == 0u) ? (kx / k_len) : (ky / k_len);
    float2 h = src_tex[id];
    // -i * (a+ib) = b - i a
    float2 disp = float2(h.y, -h.x) * (chop * k_hat);
    dst_tex[id] = disp;
}
