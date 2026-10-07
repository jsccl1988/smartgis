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
static const float kG = 9.81;

float hash01(uint i, uint j, uint salt)
{
    uint h = i * 73856093u ^ j * 19349663u ^ salt * 83492791u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h & 0x00ffffffu) / float(0x01000000u);
}

float phillips(float kx, float ky)
{
    float k2 = kx * kx + ky * ky;
    if (k2 < 1.0e-8)
        return 0.0;
    float k_len = sqrt(k2);
    float L = max((wind_speed * wind_speed) / kG, 1.0e-3);
    float kwx = cos(wind_dir_rad);
    float kwy = sin(wind_dir_rad);
    float k_dot_w = max((kx * kwx + ky * kwy) / k_len, 0.0);
    float damp = exp(-1.0 / (k2 * L * L));
    float suppress = exp(-k2 * L * L * 0.001);
    return damp * suppress * (k_dot_w * k_dot_w) / (k2 * k2);
}

// Directional JONSWAP-lite on the k-grid (shape only; amp_scale sets Hs).
float jonswap(float kx, float ky)
{
    float k2 = kx * kx + ky * ky;
    if (k2 < 1.0e-8)
        return 0.0;
    float k_len = sqrt(k2);
    float omega = sqrt(kG * k_len);
    float U = max(wind_speed, 1.0);
    float omega_p = 0.877 * kG / U;
    float alpha = 0.0081;
    float sigma = (omega <= omega_p) ? 0.07 : 0.09;
    float om_r = omega_p / max(omega, 1.0e-4);
    float pm = alpha * kG * kG * pow(omega, -5.0) * exp(-1.25 * pow(om_r, 4.0));
    float r = (omega - omega_p) / max(sigma * omega_p, 1.0e-4);
    float peak = pow(max(gamma, 1.0), exp(-0.5 * r * r));
    float S_omega = pm * peak;
    float domega_dk = 0.5 * sqrt(kG / k_len);
    float S_k = S_omega * domega_dk / k_len;
    float kwx = cos(wind_dir_rad);
    float kwy = sin(wind_dir_rad);
    float k_dot_w = max((kx * kwx + ky * kwy) / k_len, 0.0);
    return S_k * (k_dot_w * k_dot_w);
}

RWTexture2D<float2> spectrum_uav : register(u0);
RWTexture2D<float2> spectrum_seed_uav : register(u1);

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
        spectrum_uav[id] = float2(0.0, 0.0);
        spectrum_seed_uav[id] = float2(0.0, 0.0);
        return;
    }
    float dk = 2.0 * kPi / max(patch_size, 1.0);
    float kx = float(ii) * dk;
    float ky = float(jj) * dk;
    float p = (spectrum_model == 0u) ? phillips(kx, ky) : jonswap(kx, ky);
    if (p <= 0.0)
    {
        spectrum_uav[id] = float2(0.0, 0.0);
        spectrum_seed_uav[id] = float2(0.0, 0.0);
        return;
    }
    float a = amp_scale * sqrt(p * 0.5);
    float phase0 = hash01(id.x, id.y, 1u) * 2.0 * kPi;
    float k_len = sqrt(kx * kx + ky * ky);
    float omega = sqrt(kG * k_len);
    float phase = phase0 + omega * time_sec;
    float2 h = float2(a * cos(phase), a * sin(phase));
    spectrum_uav[id] = h;
    spectrum_seed_uav[id] = h;
}
