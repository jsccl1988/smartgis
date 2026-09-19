// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_PROCEDURAL_H_
#define GIS_ATMOSPHERE_PROCEDURAL_H_

#include <vector>

#include "gis/atmosphere/field_store.h"
#include "gis/gis_export.h"
#include "gis/world/land_mask.h"

namespace gis {
namespace atmosphere {

// Shared knobs for procedural atmosphere / ocean field generators.
struct ProceduralOptions {
  unsigned seed = 1;
  float wind_scale = 10.f;
  float wave_hs_scale = 0.25f;
  float cloud_cover_scale = 1.f;
  float cloud_base_m = 800.f;
  float cloud_thickness_m = 2200.f;
};

// Fills kWindU / kWindV with smooth hash noise on |grid|.
struct GIS_EXPORT WindNoise {
  ProceduralOptions opts;
  void apply(FieldStore* store, const FieldGrid& grid, int priority) const;
};

// Derives kWaveHs / kWaveDir from wind samples (reads store, then writes).
struct GIS_EXPORT WaveFromWind {
  ProceduralOptions opts;
  void apply(FieldStore* store, const FieldGrid& grid, int priority) const;
};

// Fills kCloudCover / kCloudBase / kCloudTop with procedural noise.
struct GIS_EXPORT CloudNoise {
  ProceduralOptions opts;
  void apply(FieldStore* store, const FieldGrid& grid, int priority) const;
};

// Writes kSeaMask (1=sea) as the complement of land rings (sea = !land).
struct GIS_EXPORT SeaMaskFromLand {
  void apply(FieldStore* store, const FieldGrid& grid,
             const std::vector<LonLatRing>& land_rings, int priority) const;
};

// Reserved shallow-water / advection step. v1 is a no-op.
GIS_EXPORT void procedural_step(FieldStore* store, double dt_sec);

}  // namespace atmosphere
}  // namespace gis

#endif  // GIS_ATMOSPHERE_PROCEDURAL_H_
