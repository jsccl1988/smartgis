// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_OCEAN_SYSTEM_H_
#define GIS_ATMOSPHERE_OCEAN_SYSTEM_H_

#include <cstddef>

#include "gis/atmosphere/field_store.h"
#include "gis/gis_export.h"

namespace gis {
namespace atmosphere {

// Phillips / JONSWAP-lite spectrum knobs derived from Hs, wave dir, and wind.
struct OceanSpectrumParams {
  float significant_wave_height = 0.0f;
  float mean_direction_rad = 0.0f;
  float wind_speed = 0.0f;
  float wind_direction_rad = 0.0f;
  // Power-of-two FFT resolution hint (16..128); 0 forces Gerstner fallback.
  int fft_size = 64;
  bool use_gerstner_fallback = false;
  // Peak-enhanced directional JONSWAP when true; Phillips when false.
  bool use_jonswap = true;
  float chop = 1.0f;
  float jonswap_gamma = 3.3f;
};

// One lon/lat tile of ocean fields ready for GPU upload / OceanPass.
struct OceanTileParams {
  FieldGrid extent;
  float hs = 0.0f;
  float wave_dir_rad = 0.0f;
  float wind_u = 0.0f;
  float wind_v = 0.0f;
  float sea_mask = 1.0f;
  OceanSpectrumParams spectrum;
};

// Reads FieldStore wave/wind/sea-mask channels and exposes tile + spectrum
// parameters for render::atmosphere::OceanPass.
class GIS_EXPORT OceanSystem {
 public:
  OceanSystem();
  ~OceanSystem();

  OceanSystem(const OceanSystem&) = delete;
  OceanSystem& operator=(const OceanSystem&) = delete;

  void set_quality(int quality);
  int quality() const { return quality_; }

  // Sample fields at the tile center (and optional corner average for mask).
  OceanTileParams sample_tile(const FieldStore& store, const FieldGrid& extent,
                              double time_sec) const;

  // Fill a regular grid of sea-mask samples for FieldTexture upload.
  // out size must be cols*rows; returns false on bad args.
  bool fill_sea_mask_grid(const FieldStore& store, const FieldGrid& extent,
                          int cols, int rows, double time_sec,
                          float* out_mask, std::size_t out_count) const;

  // Fill Hs grid (same layout) for displacement amplitude modulation.
  bool fill_hs_grid(const FieldStore& store, const FieldGrid& extent, int cols,
                    int rows, double time_sec, float* out_hs,
                    std::size_t out_count) const;

 private:
  int quality_ = 1;
};

// Pure helper: Hs / direction / wind → spectrum params (unit-testable).
GIS_EXPORT OceanSpectrumParams make_ocean_spectrum(float hs,
                                                   float wave_dir_rad,
                                                   float wind_u, float wind_v,
                                                   int quality);

}  // namespace atmosphere
}  // namespace gis

#endif  // GIS_ATMOSPHERE_OCEAN_SYSTEM_H_
