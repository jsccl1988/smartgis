// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/atmosphere/ocean_pass.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "render/atmosphere/field_texture.h"
#include "render/rhi/rhi.h"

namespace render {
namespace atmosphere {
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
  const uint32_t h =
      hash_u32(static_cast<uint32_t>(i) * 73856093u ^
               static_cast<uint32_t>(j) * 19349663u ^
               static_cast<uint32_t>(salt) * 83492791u);
  return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(hi, v));
}

int next_pow2_clamped(int n, int lo, int hi) {
  int v = std::max(lo, std::min(hi, n));
  int p = 1;
  while (p < v) {
    p <<= 1;
  }
  return std::min(p, hi);
}

void fft_radix2(std::vector<float>& re, std::vector<float>& im, bool inverse) {
  const int n = static_cast<int>(re.size());
  if (n < 2 || (n & (n - 1)) != 0 || im.size() != re.size()) {
    return;
  }
  // Bit-reverse permutation.
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
        const float vr =
            re[static_cast<std::size_t>(v)] * w_re - im[static_cast<std::size_t>(v)] * w_im;
        const float vi =
            re[static_cast<std::size_t>(v)] * w_im + im[static_cast<std::size_t>(v)] * w_re;
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
      row_re[static_cast<std::size_t>(x)] =
          re[static_cast<std::size_t>(y * n + x)];
      row_im[static_cast<std::size_t>(x)] =
          im[static_cast<std::size_t>(y * n + x)];
    }
    fft_radix2(row_re, row_im, true);
    for (int x = 0; x < n; ++x) {
      re[static_cast<std::size_t>(y * n + x)] = row_re[static_cast<std::size_t>(x)];
      im[static_cast<std::size_t>(y * n + x)] = row_im[static_cast<std::size_t>(x)];
    }
  }
  for (int x = 0; x < n; ++x) {
    for (int y = 0; y < n; ++y) {
      row_re[static_cast<std::size_t>(y)] =
          re[static_cast<std::size_t>(y * n + x)];
      row_im[static_cast<std::size_t>(y)] =
          im[static_cast<std::size_t>(y * n + x)];
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

// Directional JONSWAP-lite on the discrete k-grid (shape only).
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
  const float r =
      (omega - omega_p) / std::max(sigma * omega_p, 1.0e-4f);
  const float peak =
      std::pow(std::max(gamma, 1.0f), std::exp(-0.5f * r * r));
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

// Match GPU IFFT 1/N² scaling: σ = amp_scale * sqrt(Σ P/2) / N², Hs = 4σ.
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
                      double time_sec, bool use_jonswap, float gamma,
                      float chop, std::vector<float>* heights,
                      std::vector<float>* disp_x, std::vector<float>* disp_z,
                      float* out_height_scale, float* out_disp_scale) {
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
      const float phase =
          phase0 + omega * static_cast<float>(time_sec);
      const float hr = a * std::cos(phase);
      const float hi = a * std::sin(phase);
      const std::size_t idx =
          static_cast<std::size_t>(j * n + i);
      h_re[idx] = hr;
      h_im[idx] = hi;
      // -i * (hr + i hi) = hi - i hr
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
                            std::vector<float>* heights,
                            std::vector<float>* disp_x,
                            std::vector<float>* disp_z) {
  const int verts = mesh_n * mesh_n;
  heights->assign(static_cast<std::size_t>(verts), 0.0f);
  disp_x->assign(static_cast<std::size_t>(verts), 0.0f);
  disp_z->assign(static_cast<std::size_t>(verts), 0.0f);
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
      const std::size_t idx =
          static_cast<std::size_t>(jz * mesh_n + ix);
      (*disp_x)[idx] = hx;
      (*disp_z)[idx] = hz;
      (*heights)[idx] = hy;
    }
  }
}

float sample_bilinear(const std::vector<float>& grid, int cols, int rows,
                      float u, float v) {
  if (grid.empty() || cols < 1 || rows < 1) {
    return 1.0f;
  }
  u = clampf(u, 0.0f, 1.0f);
  v = clampf(v, 0.0f, 1.0f);
  const float x = u * static_cast<float>(cols - 1);
  const float y = v * static_cast<float>(rows - 1);
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(x0 + 1, cols - 1);
  const int y1 = std::min(y0 + 1, rows - 1);
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);
  const float a =
      grid[static_cast<std::size_t>(y0 * cols + x0)];
  const float b =
      grid[static_cast<std::size_t>(y0 * cols + x1)];
  const float c =
      grid[static_cast<std::size_t>(y1 * cols + x0)];
  const float d =
      grid[static_cast<std::size_t>(y1 * cols + x1)];
  const float ab = a * (1.0f - tx) + b * tx;
  const float cd = c * (1.0f - tx) + d * tx;
  return ab * (1.0f - ty) + cd * ty;
}

}  // namespace

OceanPass::OceanPass() = default;

OceanPass::~OceanPass() {
  release();
}

void OceanPass::set_params(const OceanDrawParams& params) {
  params_ = params;
  if (params_.mesh_resolution < 2) {
    params_.mesh_resolution = 2;
  }
  if (params_.fft_size > 0) {
    params_.fft_size = next_pow2_clamped(params_.fft_size, 16, 128);
  }
}

void OceanPass::set_sea_mask_texture(FieldTexture* mask) {
  sea_mask_tex_ = mask;
}

void OceanPass::set_sea_mask_cpu(int cols, int rows, const float* values,
                                 std::size_t value_count) {
  mask_cols_ = 0;
  mask_rows_ = 0;
  mask_cpu_.clear();
  if (!values || cols < 1 || rows < 1) {
    return;
  }
  const std::size_t need =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  if (value_count != need) {
    return;
  }
  mask_cols_ = cols;
  mask_rows_ = rows;
  mask_cpu_.assign(values, values + value_count);
}

float OceanPass::sample_sea_mask(float u, float v) const {
  if (!mask_cpu_.empty()) {
    return sample_bilinear(mask_cpu_, mask_cols_, mask_rows_, u, v);
  }
  // No mask → treat as open sea.
  return 1.0f;
}

void OceanPass::rebuild_mesh_grid() {
  const int mesh_n = params_.mesh_resolution;
  vertex_count_ = static_cast<uint32_t>(mesh_n * mesh_n);
  positions_.assign(static_cast<std::size_t>(vertex_count_ * 5u), 0.0f);
  const float half = params_.patch_half_extent;
  const float step = (half * 2.0f) / static_cast<float>(mesh_n - 1);
  for (int jz = 0; jz < mesh_n; ++jz) {
    for (int ix = 0; ix < mesh_n; ++ix) {
      const std::size_t vi =
          static_cast<std::size_t>(jz * mesh_n + ix);
      const float x = -half + static_cast<float>(ix) * step;
      const float z = -half + static_cast<float>(jz) * step;
      const float u = static_cast<float>(ix) / static_cast<float>(mesh_n - 1);
      const float v = static_cast<float>(jz) / static_cast<float>(mesh_n - 1);
      positions_[vi * 5 + 0] = x;
      positions_[vi * 5 + 1] = 0.f;
      positions_[vi * 5 + 2] = z;
      positions_[vi * 5 + 3] = u;
      positions_[vi * 5 + 4] = v;
    }
  }
}

void OceanPass::rebuild_displacement() {
  const int mesh_n = params_.mesh_resolution;
  rebuild_mesh_grid();
  heights_.assign(static_cast<std::size_t>(vertex_count_), 0.0f);
  disp_x_.assign(static_cast<std::size_t>(vertex_count_), 0.0f);
  disp_z_.assign(static_cast<std::size_t>(vertex_count_), 0.0f);

  const float half = params_.patch_half_extent;

  std::vector<float> heights;
  std::vector<float> disp_x;
  std::vector<float> disp_z;

  const bool use_gerstner =
      params_.use_gerstner_fallback || params_.fft_size < 16;
  if (use_gerstner) {
    build_gerstner_heights(mesh_n, params_.significant_wave_height,
                           params_.mean_direction_rad, params_.wind_speed,
                           time_sec_, half, &heights, &disp_x, &disp_z);
    float max_abs = 0.05f;
    float max_disp = 0.05f;
    for (std::size_t i = 0; i < heights.size(); ++i) {
      max_abs = std::max(max_abs, std::fabs(heights[i]));
      max_disp = std::max(max_disp, std::fabs(disp_x[i]));
      max_disp = std::max(max_disp, std::fabs(disp_z[i]));
    }
    height_scale_ = max_abs;
    disp_scale_ = max_disp;
  } else {
    const int n = params_.fft_size;
    std::vector<float> fft_h;
    std::vector<float> fft_dx;
    std::vector<float> fft_dz;
    build_fft_fields(n, params_.significant_wave_height, params_.wind_speed,
                     params_.wind_direction_rad, time_sec_, params_.use_jonswap,
                     params_.jonswap_gamma, params_.chop, &fft_h, &fft_dx,
                     &fft_dz, &height_scale_, &disp_scale_);
    heights.assign(static_cast<std::size_t>(mesh_n * mesh_n), 0.0f);
    disp_x.assign(heights.size(), 0.0f);
    disp_z.assign(heights.size(), 0.0f);
    for (int jz = 0; jz < mesh_n; ++jz) {
      for (int ix = 0; ix < mesh_n; ++ix) {
        const float u = static_cast<float>(ix) / static_cast<float>(mesh_n - 1);
        const float v = static_cast<float>(jz) / static_cast<float>(mesh_n - 1);
        const float fx = u * static_cast<float>(n - 1);
        const float fy = v * static_cast<float>(n - 1);
        const int x0 = static_cast<int>(fx);
        const int y0 = static_cast<int>(fy);
        const int x1 = std::min(x0 + 1, n - 1);
        const int y1 = std::min(y0 + 1, n - 1);
        const float tx = fx - static_cast<float>(x0);
        const float ty = fy - static_cast<float>(y0);
        auto sample4 = [&](const std::vector<float>& g) {
          const float h00 = g[static_cast<std::size_t>(y0 * n + x0)];
          const float h10 = g[static_cast<std::size_t>(y0 * n + x1)];
          const float h01 = g[static_cast<std::size_t>(y1 * n + x0)];
          const float h11 = g[static_cast<std::size_t>(y1 * n + x1)];
          return (h00 * (1.0f - tx) + h10 * tx) * (1.0f - ty) +
                 (h01 * (1.0f - tx) + h11 * tx) * ty;
        };
        const std::size_t vi =
            static_cast<std::size_t>(jz * mesh_n + ix);
        heights[vi] = sample4(fft_h);
        disp_x[vi] = sample4(fft_dx);
        disp_z[vi] = sample4(fft_dz);
      }
    }
  }

  heights_ = heights;
  disp_x_ = disp_x;
  disp_z_ = disp_z;

  // Keep base grid planar; VS applies height + Dx/Dz from the height map.
}

void OceanPass::rebuild_indices_with_mask() {
  const int mesh_n = params_.mesh_resolution;
  indices_.clear();
  indices_.reserve(static_cast<std::size_t>((mesh_n - 1) * (mesh_n - 1) * 6));
  const float thr = params_.sea_mask_threshold;
  for (int jz = 0; jz < mesh_n - 1; ++jz) {
    for (int ix = 0; ix < mesh_n - 1; ++ix) {
      const float u0 = static_cast<float>(ix) / static_cast<float>(mesh_n - 1);
      const float v0 = static_cast<float>(jz) / static_cast<float>(mesh_n - 1);
      const float u1 =
          static_cast<float>(ix + 1) / static_cast<float>(mesh_n - 1);
      const float v1 =
          static_cast<float>(jz + 1) / static_cast<float>(mesh_n - 1);
      const float m00 = sample_sea_mask(u0, v0);
      const float m10 = sample_sea_mask(u1, v0);
      const float m01 = sample_sea_mask(u0, v1);
      const float m11 = sample_sea_mask(u1, v1);
      // Discard quad if all corners are land.
      if (m00 < thr && m10 < thr && m01 < thr && m11 < thr) {
        continue;
      }
      const uint32_t i00 =
          static_cast<uint32_t>(jz * mesh_n + ix);
      const uint32_t i10 =
          static_cast<uint32_t>(jz * mesh_n + ix + 1);
      const uint32_t i01 =
          static_cast<uint32_t>((jz + 1) * mesh_n + ix);
      const uint32_t i11 =
          static_cast<uint32_t>((jz + 1) * mesh_n + ix + 1);
      indices_.push_back(i00);
      indices_.push_back(i10);
      indices_.push_back(i11);
      indices_.push_back(i00);
      indices_.push_back(i11);
      indices_.push_back(i01);
    }
  }
  index_count_ = static_cast<uint32_t>(indices_.size());
}

void OceanPass::fresnel_tint(const rhi::CameraMatrices* camera,
                             float* out_rgba) const {
  // Approximate Fresnel from camera forward vs. up (view-dependent water tint).
  float facing = 0.35f;
  if (camera) {
    // View matrix column 2 ≈ camera forward in many conventions; use Z row.
    const float fx = camera->view[2];
    const float fy = camera->view[6];
    const float fz = camera->view[10];
    const float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (flen > 1.0e-5f) {
      const float up_dot = std::abs(fy / flen);
      facing = clampf(1.0f - up_dot, 0.0f, 1.0f);
    }
  }
  const float f = params_.fresnel_bias +
                  (1.0f - params_.fresnel_bias) *
                      std::pow(facing, params_.fresnel_power);
  out_rgba[0] =
      params_.shallow_r * (1.0f - f) + params_.deep_r * f;
  out_rgba[1] =
      params_.shallow_g * (1.0f - f) + params_.deep_g * f;
  out_rgba[2] =
      params_.shallow_b * (1.0f - f) + params_.deep_b * f;
  out_rgba[3] =
      params_.shallow_a * (1.0f - f) + params_.deep_a * f;
}

bool OceanPass::ensure_resources(rhi::Device* device) {
  if (!device) {
    return false;
  }
  const int mesh_n = params_.mesh_resolution;
  const uint32_t want_verts = static_cast<uint32_t>(mesh_n * mesh_n);
  const bool need_realloc =
      device_ != device || vertex_buffer_ == nullptr ||
      index_buffer_ == nullptr || cached_mesh_res_ != mesh_n ||
      vertex_count_ != want_verts;

  if (need_realloc) {
    if (device_ && vertex_buffer_) {
      device_->destroy_buffer(vertex_buffer_);
      vertex_buffer_ = nullptr;
    }
    if (device_ && index_buffer_) {
      device_->destroy_buffer(index_buffer_);
      index_buffer_ = nullptr;
    }
    if (device_ && height_texture_) {
      device_->destroy_texture(height_texture_);
      height_texture_ = nullptr;
      height_tex_n_ = 0;
    }
    device_ = device;
    cached_mesh_res_ = mesh_n;
    const uint32_t vb_bytes = want_verts * 5u * sizeof(float);
    const uint32_t max_idx =
        static_cast<uint32_t>((mesh_n - 1) * (mesh_n - 1) * 6);
    const uint32_t ib_bytes = max_idx * sizeof(uint32_t);
    vertex_buffer_ =
        device_->create_buffer(vb_bytes, rhi::BufferUsage::kVertex);
    index_buffer_ =
        device_->create_buffer(ib_bytes, rhi::BufferUsage::kIndex);
    if (!vertex_buffer_ || !index_buffer_) {
      return false;
    }
  }
  return true;
}

bool OceanPass::ensure_fft_textures(rhi::Device* device, int n) {
  if (!device || n < 16) {
    return false;
  }
  const bool need_spectrum = !spectrum_a_ || !spectrum_b_ || !spectrum_seed_ ||
                             spectrum_n_ != n || device_ != device;
  if (need_spectrum) {
    if (spectrum_a_ && device_) {
      device_->destroy_texture(spectrum_a_);
      spectrum_a_ = nullptr;
    }
    if (spectrum_b_ && device_) {
      device_->destroy_texture(spectrum_b_);
      spectrum_b_ = nullptr;
    }
    if (spectrum_seed_ && device_) {
      device_->destroy_texture(spectrum_seed_);
      spectrum_seed_ = nullptr;
    }
    rhi::TextureDesc spec;
    spec.width = static_cast<uint32_t>(n);
    spec.height = static_cast<uint32_t>(n);
    spec.format = rhi::TextureFormat::kRg32Float;
    spec.usage = rhi::TextureUsage::kSampled | rhi::TextureUsage::kStorage;
    spectrum_a_ = device->create_texture(spec);
    spectrum_b_ = device->create_texture(spec);
    spectrum_seed_ = device->create_texture(spec);
    spectrum_n_ = n;
    if (!spectrum_a_ || !spectrum_b_ || !spectrum_seed_) {
      return false;
    }
  }

  // Height map must be UAV-writable for the encode pass, then sampled in VS.
  const bool need_height =
      !height_texture_ || height_tex_n_ != n || device_ != device;
  if (need_height) {
    if (height_texture_ && device_) {
      device_->destroy_texture(height_texture_);
      height_texture_ = nullptr;
    }
    rhi::TextureDesc desc;
    desc.width = static_cast<uint32_t>(n);
    desc.height = static_cast<uint32_t>(n);
    desc.format = rhi::TextureFormat::kRgba8;
    desc.usage = rhi::TextureUsage::kSampled | rhi::TextureUsage::kStorage |
                 rhi::TextureUsage::kCopyDest;
    height_texture_ = device->create_texture(desc);
    height_tex_n_ = n;
    if (!height_texture_) {
      return false;
    }
  }
  return true;
}

bool OceanPass::record_gpu_fft(rhi::Device* device, rhi::CommandList* list,
                               int n) {
  if (!device || !list || !device->supports_compute()) {
    return false;
  }
  if (!ensure_fft_textures(device, n)) {
    return false;
  }

  uint32_t log2_n = 0;
  for (int v = n; v > 1; v >>= 1) {
    ++log2_n;
  }

  constexpr float kPatch = 100.0f;
  const float hs = std::max(0.05f, params_.significant_wave_height);
  const float wind = std::max(1.0f, params_.wind_speed);
  const float amp_scale = compute_energy_amp_scale(
      n, kPatch, hs, wind, params_.wind_direction_rad, params_.use_jonswap,
      params_.jonswap_gamma);
  height_scale_ = std::max(0.05f, hs * 0.55f);
  disp_scale_ =
      std::max(0.05f, hs * 0.45f * std::max(params_.chop, 0.0f));

  rhi::OceanFftGpuParams p;
  p.size = static_cast<uint32_t>(n);
  p.log2_size = log2_n;
  p.stage = 0;
  p.direction = 0;
  p.time_sec = static_cast<float>(time_sec_);
  p.wind_speed = wind;
  p.wind_dir_rad = params_.wind_direction_rad;
  p.amp_scale = amp_scale;
  p.patch_size = kPatch;
  p.height_scale = height_scale_;
  p.disp_scale = disp_scale_;
  p.chop = params_.chop;
  p.spectrum_model = params_.use_jonswap
                         ? static_cast<uint32_t>(rhi::OceanSpectrumModel::kJonswap)
                         : static_cast<uint32_t>(rhi::OceanSpectrumModel::kPhillips);
  p.encode_channel = 0;
  p.gamma = params_.jonswap_gamma;

  const uint32_t groups = (static_cast<uint32_t>(n) + 7u) / 8u;

  auto run_1d = [&](uint32_t direction, rhi::Texture* src_start) {
    rhi::Texture* src = src_start;
    rhi::Texture* dst = (src_start == spectrum_a_) ? spectrum_b_ : spectrum_a_;
    p.direction = direction;
    p.stage = 0;
    list->set_compute_pipeline(rhi::ComputePipelineId::kOceanFftBitReverse);
    list->set_ocean_fft_params(p);
    list->bind_compute_srv(src, 0);
    list->bind_compute_uav(dst, 0);
    list->dispatch(groups, groups, 1);
    list->uav_barrier();
    std::swap(src, dst);

    list->set_compute_pipeline(rhi::ComputePipelineId::kOceanFftButterfly);
    for (uint32_t stage = 0; stage < log2_n; ++stage) {
      p.stage = stage;
      list->set_ocean_fft_params(p);
      list->bind_compute_srv(src, 0);
      list->bind_compute_uav(dst, 0);
      list->dispatch(groups, groups, 1);
      list->uav_barrier();
      std::swap(src, dst);
    }
    return src;
  };

  auto encode_channel = [&](rhi::Texture* field, uint32_t channel) {
    p.encode_channel = channel;
    list->set_compute_pipeline(rhi::ComputePipelineId::kOceanHeightEncode);
    list->set_ocean_fft_params(p);
    list->bind_compute_srv(field, 0);
    list->bind_compute_uav(height_texture_, 0);
    list->dispatch(groups, groups, 1);
    list->uav_barrier();
  };

  // 1) Spectrum → spectrum_a_ + spectrum_seed_
  list->set_compute_pipeline(rhi::ComputePipelineId::kOceanSpectrum);
  list->set_ocean_fft_params(p);
  list->bind_compute_uav(spectrum_a_, 0);
  list->bind_compute_uav(spectrum_seed_, 1);
  list->dispatch(groups, groups, 1);
  list->uav_barrier();

  // 2) Height IFFT + encode R
  rhi::Texture* after_rows = run_1d(0, spectrum_a_);
  rhi::Texture* height_field = run_1d(1, after_rows);
  encode_channel(height_field, 0);

  // 3) Dx from seed, IFFT, encode G
  p.direction = 0;
  list->set_compute_pipeline(
      rhi::ComputePipelineId::kOceanDisplacementSpectrum);
  list->set_ocean_fft_params(p);
  list->bind_compute_srv(spectrum_seed_, 0);
  list->bind_compute_uav(spectrum_a_, 0);
  list->dispatch(groups, groups, 1);
  list->uav_barrier();
  after_rows = run_1d(0, spectrum_a_);
  rhi::Texture* dx_field = run_1d(1, after_rows);
  encode_channel(dx_field, 1);

  // 4) Dz from seed, IFFT, encode B
  p.direction = 1;
  list->set_compute_pipeline(
      rhi::ComputePipelineId::kOceanDisplacementSpectrum);
  list->set_ocean_fft_params(p);
  list->bind_compute_srv(spectrum_seed_, 0);
  list->bind_compute_uav(spectrum_a_, 0);
  list->dispatch(groups, groups, 1);
  list->uav_barrier();
  after_rows = run_1d(0, spectrum_a_);
  rhi::Texture* dz_field = run_1d(1, after_rows);
  encode_channel(dz_field, 2);

  return true;
}

bool OceanPass::upload_height_texture(rhi::Device* device) {
  if (!device || heights_.empty()) {
    return false;
  }
  const int n = params_.mesh_resolution;
  if (n < 2) {
    return false;
  }
  if (!height_texture_ || height_tex_n_ != n || device_ != device) {
    if (height_texture_ && device_) {
      device_->destroy_texture(height_texture_);
      height_texture_ = nullptr;
    }
    rhi::TextureDesc desc;
    desc.width = static_cast<uint32_t>(n);
    desc.height = static_cast<uint32_t>(n);
    desc.format = rhi::TextureFormat::kRgba8;
    desc.usage = rhi::TextureUsage::kSampled | rhi::TextureUsage::kCopyDest;
    height_texture_ = device->create_texture(desc);
    height_tex_n_ = n;
    if (!height_texture_) {
      return false;
    }
  }
  const float h_scale = std::max(height_scale_, 1.0e-3f);
  const float d_scale = std::max(disp_scale_, 1.0e-3f);
  std::vector<uint8_t> rgba(static_cast<std::size_t>(n * n * 4), 0);
  for (int i = 0; i < n * n; ++i) {
    const float h = heights_[static_cast<std::size_t>(i)];
    const float dx =
        disp_x_.empty() ? 0.f : disp_x_[static_cast<std::size_t>(i)];
    const float dz =
        disp_z_.empty() ? 0.f : disp_z_[static_cast<std::size_t>(i)];
    const float enc_h = clampf(0.5f + 0.5f * (h / h_scale), 0.0f, 1.0f);
    const float enc_x = clampf(0.5f + 0.5f * (dx / d_scale), 0.0f, 1.0f);
    const float enc_z = clampf(0.5f + 0.5f * (dz / d_scale), 0.0f, 1.0f);
    rgba[static_cast<std::size_t>(i * 4 + 0)] =
        static_cast<uint8_t>(enc_h * 255.0f + 0.5f);
    rgba[static_cast<std::size_t>(i * 4 + 1)] =
        static_cast<uint8_t>(enc_x * 255.0f + 0.5f);
    rgba[static_cast<std::size_t>(i * 4 + 2)] =
        static_cast<uint8_t>(enc_z * 255.0f + 0.5f);
    rgba[static_cast<std::size_t>(i * 4 + 3)] = 255;
  }
  return device->upload_texture(height_texture_, rgba.data(),
                                static_cast<uint32_t>(rgba.size()));
}

bool OceanPass::record(rhi::Device* device, rhi::CommandList* list,
                       uint32_t width, uint32_t height,
                       const rhi::CameraMatrices* camera) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  used_gpu_fft_ = false;

  const bool want_gpu =
      params_.prefer_gpu_fft && !params_.use_gerstner_fallback &&
      params_.fft_size >= 16 && device->supports_compute();

  if (want_gpu) {
    rebuild_mesh_grid();
    const float hs = std::max(0.05f, params_.significant_wave_height);
    height_scale_ = std::max(0.05f, hs * 0.55f);
    disp_scale_ =
        std::max(0.05f, hs * 0.45f * std::max(params_.chop, 0.0f));
  } else {
    rebuild_displacement();
  }

  rebuild_indices_with_mask();
  if (index_count_ == 0) {
    return true;
  }
  if (!ensure_resources(device)) {
    return false;
  }

  if (want_gpu) {
    if (!record_gpu_fft(device, list, params_.fft_size)) {
      // Compute unavailable at record time — fall back to CPU FFT.
      rebuild_displacement();
      if (!upload_height_texture(device)) {
        return false;
      }
    } else {
      used_gpu_fft_ = true;
    }
  } else {
    if (!upload_height_texture(device)) {
      return false;
    }
  }

  const uint32_t vb_bytes =
      static_cast<uint32_t>(positions_.size() * sizeof(float));
  const uint32_t ib_bytes =
      static_cast<uint32_t>(indices_.size() * sizeof(uint32_t));
  if (!device->upload(vertex_buffer_, positions_.data(), vb_bytes) ||
      !device->upload(index_buffer_, indices_.data(), ib_bytes)) {
    return false;
  }

  float rgba[4];
  fresnel_tint(camera, rgba);

  rhi::OceanGpuParams ocean;
  ocean.deep_r = params_.deep_r;
  ocean.deep_g = params_.deep_g;
  ocean.deep_b = params_.deep_b;
  ocean.deep_a = params_.deep_a;
  ocean.shallow_r = params_.shallow_r;
  ocean.shallow_g = params_.shallow_g;
  ocean.shallow_b = params_.shallow_b;
  ocean.shallow_a = params_.shallow_a;
  ocean.fresnel_bias = params_.fresnel_bias;
  ocean.fresnel_power = params_.fresnel_power;
  ocean.height_scale = height_scale_;
  ocean.disp_scale = disp_scale_;
  if (camera) {
    // Orbit cameras put the eye translation in the view matrix translation.
    ocean.cam_x = -camera->view[12];
    ocean.cam_y = -camera->view[13];
    ocean.cam_z = -camera->view[14];
  }

  list->set_viewport(0, 0, static_cast<float>(width),
                     static_cast<float>(height), 0, 1);
  if (camera) {
    list->bind_camera(*camera);
  }
  list->set_pipeline(rhi::PipelineId::kOcean);
  list->set_blend_mode(rhi::BlendMode::kOpaque);
  list->set_depth_mode(rhi::DepthMode::kWrite);
  list->set_ocean_params(ocean);
  list->bind_texture(height_texture_, 0);
  // Solid color remains a Null/fallback tint signal for tests.
  list->set_solid_color(rgba[0], rgba[1], rgba[2], rgba[3]);
  list->bind_vertex_buffer(vertex_buffer_, 0, 5 * sizeof(float));
  list->bind_index_buffer(index_buffer_, 0);
  list->draw_indexed(index_count_, 1, 0, 0, 0);
  return true;
}

void OceanPass::release() {
  if (device_) {
    if (vertex_buffer_) {
      device_->destroy_buffer(vertex_buffer_);
      vertex_buffer_ = nullptr;
    }
    if (index_buffer_) {
      device_->destroy_buffer(index_buffer_);
      index_buffer_ = nullptr;
    }
    if (height_texture_) {
      device_->destroy_texture(height_texture_);
      height_texture_ = nullptr;
    }
    if (spectrum_a_) {
      device_->destroy_texture(spectrum_a_);
      spectrum_a_ = nullptr;
    }
    if (spectrum_b_) {
      device_->destroy_texture(spectrum_b_);
      spectrum_b_ = nullptr;
    }
    if (spectrum_seed_) {
      device_->destroy_texture(spectrum_seed_);
      spectrum_seed_ = nullptr;
    }
  }
  device_ = nullptr;
  index_count_ = 0;
  vertex_count_ = 0;
  cached_mesh_res_ = 0;
  height_tex_n_ = 0;
  spectrum_n_ = 0;
  used_gpu_fft_ = false;
  positions_.clear();
  heights_.clear();
  disp_x_.clear();
  disp_z_.clear();
  indices_.clear();
  sea_mask_tex_ = nullptr;
  mask_cols_ = 0;
  mask_rows_ = 0;
  mask_cpu_.clear();
}

}  // namespace atmosphere
}  // namespace render
