// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/atmosphere/ocean/cpu_waves.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "vista/atmosphere/detail/math.h"

namespace vista {
namespace detail {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kG = 9.81f;

uint32_t hash_u32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

float hash_float(int i, int j, int salt) {
  const uint32_t h = hash_u32(static_cast<uint32_t>(i) * 73856093u ^
                              static_cast<uint32_t>(j) * 19349663u ^
                              static_cast<uint32_t>(salt) * 83492791u);
  return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

void fft_radix2(std::vector<float>& re, std::vector<float>& im, bool inverse) {
  const int n = static_cast<int>(re.size());
  if (n < 2 || (n & (n - 1)) != 0 || im.size() != re.size()) {
    return;
  }
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap(re[static_cast<std::size_t>(i)], re[static_cast<std::size_t>(j)]);
      std::swap(im[static_cast<std::size_t>(i)], im[static_cast<std::size_t>(j)]);
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    const float ang = (inverse ? 2.0f : -2.0f) * kPi / static_cast<float>(len);
    const float wlen_re = std::cos(ang);
    const float wlen_im = std::sin(ang);
    for (int i = 0; i < n; i += len) {
      float w_re = 1.0f;
      float w_im = 0.0f;
      for (int j = 0; j < len / 2; ++j) {
        const int u = i + j;
        const int v = i + j + len / 2;
        const float ur = re[static_cast<std::size_t>(u)];
        const float ui = im[static_cast<std::size_t>(u)];
        const float vr = re[static_cast<std::size_t>(v)] * w_re -
                         im[static_cast<std::size_t>(v)] * w_im;
        const float vi = re[static_cast<std::size_t>(v)] * w_im +
                         im[static_cast<std::size_t>(v)] * w_re;
        re[static_cast<std::size_t>(u)] = ur + vr;
        im[static_cast<std::size_t>(u)] = ui + vi;
        re[static_cast<std::size_t>(v)] = ur - vr;
        im[static_cast<std::size_t>(v)] = ui - vi;
        const float next_w_re = w_re * wlen_re - w_im * wlen_im;
        w_im = w_re * wlen_im + w_im * wlen_re;
        w_re = next_w_re;
      }
    }
  }
  if (inverse) {
    const float inv = 1.0f / static_cast<float>(n);
    for (int i = 0; i < n; ++i) {
      re[static_cast<std::size_t>(i)] *= inv;
      im[static_cast<std::size_t>(i)] *= inv;
    }
  }
}

void fft2d_inverse(std::vector<float>& re, std::vector<float>& im, int n) {
  std::vector<float> row_re(static_cast<std::size_t>(n));
  std::vector<float> row_im(static_cast<std::size_t>(n));
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      row_re[static_cast<std::size_t>(x)] = re[static_cast<std::size_t>(y * n + x)];
      row_im[static_cast<std::size_t>(x)] = im[static_cast<std::size_t>(y * n + x)];
    }
    fft_radix2(row_re, row_im, true);
    for (int x = 0; x < n; ++x) {
      re[static_cast<std::size_t>(y * n + x)] = row_re[static_cast<std::size_t>(x)];
      im[static_cast<std::size_t>(y * n + x)] = row_im[static_cast<std::size_t>(x)];
    }
  }
  for (int x = 0; x < n; ++x) {
    for (int y = 0; y < n; ++y) {
      row_re[static_cast<std::size_t>(y)] = re[static_cast<std::size_t>(y * n + x)];
      row_im[static_cast<std::size_t>(y)] = im[static_cast<std::size_t>(y * n + x)];
    }
    fft_radix2(row_re, row_im, true);
    for (int y = 0; y < n; ++y) {
      re[static_cast<std::size_t>(y * n + x)] = row_re[static_cast<std::size_t>(y)];
      im[static_cast<std::size_t>(y * n + x)] = row_im[static_cast<std::size_t>(y)];
    }
  }
}

float phillips(float kx, float ky, float wind_speed, float wind_dir_rad) {
  const float k2 = kx * kx + ky * ky;
  if (k2 < 1.0e-8f) {
    return 0.0f;
  }
  const float k_len = std::sqrt(k2);
  const float L = (wind_speed * wind_speed) / kG;
  const float kwx = std::cos(wind_dir_rad);
  const float kwy = std::sin(wind_dir_rad);
  float k_dot_w = (kx * kwx + ky * kwy) / k_len;
  if (k_dot_w < 0.0f) {
    k_dot_w = 0.0f;
  }
  const float damp = std::exp(-1.0f / (k2 * L * L));
  const float suppress = std::exp(-k2 * L * L * 0.001f);
  return damp * suppress * (k_dot_w * k_dot_w) / (k2 * k2);
}

float jonswap(float kx, float ky, float wind_speed, float wind_dir_rad,
              float gamma) {
  const float k2 = kx * kx + ky * ky;
  if (k2 < 1.0e-8f) {
    return 0.0f;
  }
  const float k_len = std::sqrt(k2);
  const float omega = std::sqrt(kG * k_len);
  const float U = std::max(wind_speed, 1.0f);
  const float omega_p = 0.877f * kG / U;
  constexpr float kAlpha = 0.0081f;
  const float sigma = (omega <= omega_p) ? 0.07f : 0.09f;
  const float om_r = omega_p / std::max(omega, 1.0e-4f);
  const float pm = kAlpha * kG * kG * std::pow(omega, -5.0f) *
                   std::exp(-1.25f * std::pow(om_r, 4.0f));
  const float r = (omega - omega_p) / std::max(sigma * omega_p, 1.0e-4f);
  const float peak = std::pow(std::max(gamma, 1.0f), std::exp(-0.5f * r * r));
  const float S_omega = pm * peak;
  const float domega_dk = 0.5f * std::sqrt(kG / k_len);
  const float S_k = S_omega * domega_dk / k_len;
  const float kwx = std::cos(wind_dir_rad);
  const float kwy = std::sin(wind_dir_rad);
  float k_dot_w = (kx * kwx + ky * kwy) / k_len;
  if (k_dot_w < 0.0f) {
    k_dot_w = 0.0f;
  }
  return S_k * (k_dot_w * k_dot_w);
}

float spectral_density(float kx, float ky, float wind_speed, float wind_dir,
                       bool use_jonswap, float gamma) {
  if (use_jonswap) {
    return jonswap(kx, ky, wind_speed, wind_dir, gamma);
  }
  return phillips(kx, ky, wind_speed, wind_dir);
}

}  // namespace

float compute_energy_amp_scale(int n, float patch, float hs, float wind_speed,
                               float wind_dir, bool use_jonswap, float gamma) {
  const float dk = 2.0f * kPi / std::max(patch, 1.0f);
  double sum_p2 = 0.0;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      const int ii = (i < n / 2) ? i : i - n;
      const int jj = (j < n / 2) ? j : j - n;
      if (ii == 0 && jj == 0) {
        continue;
      }
      const float kx = static_cast<float>(ii) * dk;
      const float ky = static_cast<float>(jj) * dk;
      const float p = spectral_density(kx, ky, std::max(1.0f, wind_speed),
                                       wind_dir, use_jonswap, gamma);
      if (p > 0.0f) {
        sum_p2 += static_cast<double>(p) * 0.5;
      }
    }
  }
  const float target_sigma = std::max(0.05f, hs) * 0.25f;
  const float denom = static_cast<float>(std::sqrt(std::max(sum_p2, 1.0e-20)));
  const float n2 = static_cast<float>(n * n);
  return target_sigma * n2 / denom;
}

void build_fft_fields(int n, float hs, float wind_speed, float wind_dir,
                      double time_sec, bool use_jonswap, float gamma, float chop,
                      std::vector<float>* heights, std::vector<float>* disp_x,
                      std::vector<float>* disp_z, float* out_height_scale,
                      float* out_disp_scale) {
  heights->assign(static_cast<std::size_t>(n * n), 0.0f);
  disp_x->assign(static_cast<std::size_t>(n * n), 0.0f);
  disp_z->assign(static_cast<std::size_t>(n * n), 0.0f);
  if (n < 4) {
    if (out_height_scale) {
      *out_height_scale = std::max(0.05f, hs * 0.55f);
    }
    if (out_disp_scale) {
      *out_disp_scale = std::max(0.05f, hs * 0.45f * std::max(chop, 0.0f));
    }
    return;
  }
  const float patch = 100.0f;
  const float dk = 2.0f * kPi / patch;
  const float amp_scale = compute_energy_amp_scale(
      n, patch, hs, wind_speed, wind_dir, use_jonswap, gamma);

  std::vector<float> h_re(static_cast<std::size_t>(n * n), 0.0f);
  std::vector<float> h_im(static_cast<std::size_t>(n * n), 0.0f);
  std::vector<float> dx_re(static_cast<std::size_t>(n * n), 0.0f);
  std::vector<float> dx_im(static_cast<std::size_t>(n * n), 0.0f);
  std::vector<float> dz_re(static_cast<std::size_t>(n * n), 0.0f);
  std::vector<float> dz_im(static_cast<std::size_t>(n * n), 0.0f);

  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      const int ii = (i < n / 2) ? i : i - n;
      const int jj = (j < n / 2) ? j : j - n;
      if (ii == 0 && jj == 0) {
        continue;
      }
      const float kx = static_cast<float>(ii) * dk;
      const float ky = static_cast<float>(jj) * dk;
      const float p = spectral_density(kx, ky, std::max(1.0f, wind_speed),
                                       wind_dir, use_jonswap, gamma);
      if (p <= 0.0f) {
        continue;
      }
      const float a = amp_scale * std::sqrt(p * 0.5f);
      const float phase0 = hash_float(i, j, 1) * 2.0f * kPi;
      const float k_len = std::sqrt(kx * kx + ky * ky);
      const float omega = std::sqrt(kG * k_len);
      const float phase = phase0 + omega * static_cast<float>(time_sec);
      const float hr = a * std::cos(phase);
      const float hi = a * std::sin(phase);
      const std::size_t idx = static_cast<std::size_t>(j * n + i);
      h_re[idx] = hr;
      h_im[idx] = hi;
      const float kx_hat = kx / k_len;
      const float ky_hat = ky / k_len;
      dx_re[idx] = chop * kx_hat * hi;
      dx_im[idx] = -chop * kx_hat * hr;
      dz_re[idx] = chop * ky_hat * hi;
      dz_im[idx] = -chop * ky_hat * hr;
    }
  }
  fft2d_inverse(h_re, h_im, n);
  fft2d_inverse(dx_re, dx_im, n);
  fft2d_inverse(dz_re, dz_im, n);
  for (int i = 0; i < n * n; ++i) {
    (*heights)[static_cast<std::size_t>(i)] = h_re[static_cast<std::size_t>(i)];
    (*disp_x)[static_cast<std::size_t>(i)] = dx_re[static_cast<std::size_t>(i)];
    (*disp_z)[static_cast<std::size_t>(i)] = dz_re[static_cast<std::size_t>(i)];
  }
  if (out_height_scale) {
    *out_height_scale = std::max(0.05f, hs * 0.55f);
  }
  if (out_disp_scale) {
    *out_disp_scale = std::max(0.05f, hs * 0.45f * std::max(chop, 0.0f));
  }
}

void build_gerstner_heights(int mesh_n, float hs, float dir_rad, float wind_speed,
                            double time_sec, float half_extent,
                            std::vector<float>* heights, std::vector<float>* disp_x,
                            std::vector<float>* disp_z) {
  const int verts = mesh_n * mesh_n;
  // Every cell is written below — resize (no zero-fill) on the present hot path.
  heights->resize(static_cast<std::size_t>(verts));
  disp_x->resize(static_cast<std::size_t>(verts));
  disp_z->resize(static_cast<std::size_t>(verts));
  const float dx = (half_extent * 2.0f) / static_cast<float>(mesh_n - 1);
  const float dz = dx;
  const float base_a = std::max(0.05f, hs) * 0.25f;
  struct Wave {
    float amp;
    float len;
    float steep;
    float dir_off;
  };
  const Wave waves[] = {
      {1.0f, 24.0f, 0.35f, 0.0f},
      {0.55f, 11.0f, 0.28f, 0.4f},
      {0.30f, 5.5f, 0.22f, -0.55f},
      {0.18f, 2.8f, 0.18f, 1.1f},
  };
  const float wind_boost = clampf(wind_speed / 10.0f, 0.4f, 2.0f);
  for (int jz = 0; jz < mesh_n; ++jz) {
    for (int ix = 0; ix < mesh_n; ++ix) {
      const float x = -half_extent + static_cast<float>(ix) * dx;
      const float z = -half_extent + static_cast<float>(jz) * dz;
      float hx = 0.0f;
      float hy = 0.0f;
      float hz = 0.0f;
      for (const Wave& w : waves) {
        const float d = dir_rad + w.dir_off;
        const float dx_w = std::cos(d);
        const float dz_w = std::sin(d);
        const float k = 2.0f * kPi / w.len;
        const float omega = std::sqrt(kG * k);
        const float a = base_a * w.amp * wind_boost;
        const float q = w.steep;
        const float phase =
            k * (dx_w * x + dz_w * z) - omega * static_cast<float>(time_sec);
        const float s = std::sin(phase);
        const float c = std::cos(phase);
        hx += q * a * dx_w * c;
        hz += q * a * dz_w * c;
        hy += a * s;
      }
      const std::size_t idx = static_cast<std::size_t>(jz * mesh_n + ix);
      (*disp_x)[idx] = hx;
      (*disp_z)[idx] = hz;
      (*heights)[idx] = hy;
    }
  }
}

}  // namespace detail
}  // namespace vista
