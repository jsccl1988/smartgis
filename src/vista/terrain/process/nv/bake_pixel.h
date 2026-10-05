// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_PROCESS_NV_BAKE_PIXEL_H_
#define VISTA_TERRAIN_PROCESS_NV_BAKE_PIXEL_H_

#ifdef __CUDACC__
#define VISTA_BAKE_HD __host__ __device__
#else
#define VISTA_BAKE_HD inline
#endif

namespace vista {
namespace detail {

VISTA_BAKE_HD float bake_clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

VISTA_BAKE_HD unsigned char bake_pack_u8(float v) {
  v = bake_clampf(v, 0.f, 1.f);
  return static_cast<unsigned char>(v * 255.f + 0.5f);
}

VISTA_BAKE_HD void hypsometric_rgb_impl(float meters, float* r, float* g,
                                        float* b) {
  if (!r || !g || !b) {
    return;
  }
  if (meters <= 1.f) {
    *r = 0.10f;
    *g = 0.18f;
    *b = 0.28f;
    return;
  }
  const float t01 = bake_clampf(meters / 5500.f, 0.f, 1.f);
  if (t01 < 0.28f) {
    const float u = t01 / 0.28f;
    *r = (58.f + 42.f * u) / 255.f;
    *g = (118.f + 36.f * u) / 255.f;
    *b = (72.f + 18.f * (1.f - u)) / 255.f;
  } else if (t01 < 0.52f) {
    const float u = (t01 - 0.28f) / 0.24f;
    *r = (100.f + 48.f * u) / 255.f;
    *g = (154.f - 18.f * u) / 255.f;
    *b = (68.f + 12.f * u) / 255.f;
  } else if (t01 < 0.78f) {
    const float u = (t01 - 0.52f) / 0.26f;
    *r = (148.f + 36.f * u) / 255.f;
    *g = (136.f - 8.f * u) / 255.f;
    *b = (80.f + 20.f * u) / 255.f;
  } else {
    const float u = (t01 - 0.78f) / 0.22f;
    *r = (164.f + 28.f * u) / 255.f;
    *g = (148.f + 22.f * u) / 255.f;
    *b = (118.f + 28.f * u) / 255.f;
  }
}

VISTA_BAKE_HD void terrain_material_rgb_impl(float meters, float slope01,
                                             float* r, float* g, float* b) {
  hypsometric_rgb_impl(meters, r, g, b);
  const float rock = bake_clampf(slope01, 0.f, 1.f);
  const float w = 0.55f * rock;
  *r = *r * (1.f - w) + 0.42f * w;
  *g = *g * (1.f - w) + 0.40f * w;
  *b = *b * (1.f - w) + 0.38f * w;
  if (meters > 4200.f && rock < 0.45f) {
    const float u =
        bake_clampf((meters - 4200.f) / 2200.f, 0.f, 1.f) * (1.f - rock);
    *r = *r * (1.f - u) + 0.55f * u;
    *g = *g * (1.f - u) + 0.58f * u;
    *b = *b * (1.f - u) + 0.62f * u;
  }
}

VISTA_BAKE_HD void jet_elevation_rgb_impl(float t01, float* r, float* g,
                                          float* b) {
  if (!r || !g || !b) {
    return;
  }
  const float t = bake_clampf(t01, 0.f, 1.f);
  const float st[7] = {0.00f, 0.16f, 0.33f, 0.50f, 0.67f, 0.84f, 1.00f};
  const float sr[7] = {0.20f, 0.00f, 0.00f, 0.10f, 0.98f, 0.95f, 0.55f};
  const float sg[7] = {0.00f, 0.00f, 0.85f, 0.85f, 0.92f, 0.12f, 0.00f};
  const float sb[7] = {0.40f, 0.90f, 0.95f, 0.15f, 0.05f, 0.05f, 0.05f};
  if (t <= st[0]) {
    *r = sr[0];
    *g = sg[0];
    *b = sb[0];
    return;
  }
  for (int i = 1; i < 7; ++i) {
    if (t <= st[i]) {
      const float u = (t - st[i - 1]) / (st[i] - st[i - 1]);
      *r = sr[i - 1] + (sr[i] - sr[i - 1]) * u;
      *g = sg[i - 1] + (sg[i] - sg[i - 1]) * u;
      *b = sb[i - 1] + (sb[i] - sb[i - 1]) * u;
      return;
    }
  }
  *r = sr[6];
  *g = sg[6];
  *b = sb[6];
}

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_PROCESS_NV_BAKE_PIXEL_H_
