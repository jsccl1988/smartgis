// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_OCEAN_CONSTANTS_H_
#define EFFECT_ATMOSPHERE_OCEAN_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace effect {
namespace atmosphere {

// Spectrum selector written into OceanFftConstants::spectrum_model.
// Phillips is 0 and JONSWAP is 1, matching the compute shader branch.
enum class OceanSpectrumModel : uint32_t {
  kPhillips = 0,
  kJonswap = 1,
};

// GPU byte layout of the ocean graphics cbuffer (HLSL OceanCB, register b1).
struct OceanConstants {
  float deep[4];
  float shallow[4];
  float fresnel_bias;
  float fresnel_power;
  float height_scale;
  float cam_x;
  float cam_y;
  float cam_z;
  float disp_scale;
  float pad1;
};

// GPU byte layout shared by the five ocean compute shaders (HLSL OceanFftCB,
// register b0).
struct OceanFftConstants {
  uint32_t size;
  uint32_t log2_size;
  uint32_t stage;
  uint32_t direction;
  float time_sec;
  float wind_speed;
  float wind_dir_rad;
  float amp_scale;
  float patch_size;
  float height_scale;
  float disp_scale;
  float chop;
  uint32_t spectrum_model;
  uint32_t encode_channel;
  float gamma;
  float pad;
};

static_assert(sizeof(OceanConstants) == 64, "OceanCB is 64 bytes");
static_assert(offsetof(OceanConstants, fresnel_bias) == 32,
              "OceanCB fresnel starts after two float4s");
static_assert(sizeof(OceanFftConstants) == 64, "OceanFftCB is 64 bytes");
static_assert(offsetof(OceanFftConstants, spectrum_model) == 48,
              "OceanFftCB spectrum_model follows three 16-byte rows");

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_OCEAN_CONSTANTS_H_
