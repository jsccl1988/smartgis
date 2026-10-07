// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_WORLD_ATMOSPHERE_OCEAN_CPU_WAVES_H_
#define VISTA_COMPONENT_WORLD_ATMOSPHERE_OCEAN_CPU_WAVES_H_

#include <vector>

namespace vista {
namespace detail {

// CPU JONSWAP/Phillips IFFT displacement. Matches GPU 1/N² energy scaling.
void build_fft_fields(int n, float hs, float wind_speed, float wind_dir,
                      double time_sec, bool use_jonswap, float gamma, float chop,
                      std::vector<float>* heights, std::vector<float>* disp_x,
                      std::vector<float>* disp_z, float* out_height_scale,
                      float* out_disp_scale);

// When |out_height_scale| / |out_disp_scale| are non-null, returns max-abs
// scales in the same write pass (warm OceanPass::record skips a second scan).
void build_gerstner_heights(int mesh_n, float hs, float dir_rad,
                            float wind_speed, double time_sec, float half_extent,
                            std::vector<float>* heights, std::vector<float>* disp_x,
                            std::vector<float>* disp_z,
                            float* out_height_scale = nullptr,
                            float* out_disp_scale = nullptr);

// σ target so Hs = 4σ after IFFT 1/N². Shared with the GPU spectrum dispatch.
float compute_energy_amp_scale(int n, float patch, float hs, float wind_speed,
                               float wind_dir, bool use_jonswap, float gamma);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_ATMOSPHERE_OCEAN_CPU_WAVES_H_
