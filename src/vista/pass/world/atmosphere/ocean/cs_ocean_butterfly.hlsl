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

float2 mul_complex(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    // Inverse radix-2 Cooley-Tukey (after bit-reverse), ping-pong.
    if (id.x >= size || id.y >= size)
        return;
    uint i = (direction == 0u) ? id.x : id.y;
    uint o = (direction == 0u) ? id.y : id.x;
    uint len = 2u << stage;
    uint half = len >> 1;
    uint base = (i / len) * len;
    uint j = i % half;
    uint u = base + j;
    uint v = base + j + half;
    bool upper = (i % len) >= half;

    float2 a = (direction == 0u) ? src_tex[uint2(u, o)] : src_tex[uint2(o, u)];
    float2 b = (direction == 0u) ? src_tex[uint2(v, o)] : src_tex[uint2(o, v)];
    float ang = +2.0 * kPi * float(j) / float(len);
    float2 w = float2(cos(ang), sin(ang));
    float2 t = mul_complex(w, b);
    float2 result = upper ? (a - t) : (a + t);
    // Match CPU inverse scale at the final stage of each 1D transform.
    if (stage + 1u == log2_size)
        result *= (1.0 / float(size));
    if (direction == 0u)
        dst_tex[uint2(i, o)] = result;
    else
        dst_tex[uint2(o, i)] = result;
}
