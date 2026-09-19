// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_CLOUD_SYSTEM_H_
#define GIS_ATMOSPHERE_CLOUD_SYSTEM_H_

#include "gis/atmosphere/field_store.h"
#include "gis/gis_export.h"

namespace gis {
namespace atmosphere {

// Point sample of cloud / wind fields for CloudPass upload and advection.
struct CloudSample {
  float cover = 0.0f;   // [0,1]
  float base_m = 1000.0f;
  float top_m = 3000.0f;
  float wind_u = 0.0f;
  float wind_v = 0.0f;
};

// Optional noise advection that writes cover back into FieldStore.
struct CloudAdvectionParams {
  bool enabled = false;
  float noise_scale = 0.02f;
  float speed_scale = 1.0f;
  // Peak cover delta applied per second of dt (clamped to [0,1] after).
  float strength = 0.05f;
};

// Reads cloud cover / base / top / wind from FieldStore; optional cover advection.
class GIS_EXPORT CloudSystem {
 public:
  CloudSystem();
  ~CloudSystem();

  CloudSystem(const CloudSystem&) = delete;
  CloudSystem& operator=(const CloudSystem&) = delete;

  // Samples FieldStore channels at lon/lat/time (clamp/lerp owned by store).
  CloudSample sample_at(const FieldStore& store, double lon, double lat,
                        double time_sec) const;

  // Pure density: cover * vertical falloff in [base,top] * noise in [0,1].
  static float density_from_cover(float cover, float height_m, float base_m,
                                  float top_m, float noise01);

  // Maps AtmosphereParams::quality to raymarch step budget.
  static int raymarch_steps_for_quality(int quality);

  // When params.enabled, advects kCloudCover cells by wind + noise and
  // writebacks via set_layer. Returns false if no cover layer or bad dt.
  bool advect_cover(FieldStore* store, const CloudAdvectionParams& params,
                    double dt_sec, double time_sec) const;
};

}  // namespace atmosphere
}  // namespace gis

#endif  // GIS_ATMOSPHERE_CLOUD_SYSTEM_H_
