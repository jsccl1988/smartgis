// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_OCEAN_CPU_WAVES_H_
#define EFFECT_ATMOSPHERE_OCEAN_CPU_WAVES_H_

#include <vector>

namespace effect {
namespace atmosphere {
namespace detail {

// CPU JONSWAP/Phillips IFFT displacement. Matches GPU 1/N² energy scaling.
void build_fft_fields(int n, float hs, float wind_speed, float wind_dir,
                      double time_sec, bool use_jonswap, float gamma, float chop,
                      std::vector<float>* heights, std::vector<float>* disp_x,
                      std::vector<float>* disp_z, float* out_height_scale,
                      float* out_disp_scale);

void build_gerstner_heights(int mesh_n, float hs, float dir_rad,
                            float wind_speed, double time_sec, float half_extent,
                            std::vector<float>* heights, std::vector<float>* disp_x,
                            std::vector<float>* disp_z);

// σ target so Hs = 4σ after IFFT 1/N². Shared with the GPU spectrum dispatch.
float compute_energy_amp_scale(int n, float patch, float hs, float wind_speed,
                               float wind_dir, bool use_jonswap, float gamma);

}  // namespace detail
}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_OCEAN_CPU_WAVES_H_
