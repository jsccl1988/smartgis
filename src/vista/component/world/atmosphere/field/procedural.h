// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_WORLD_ATMOSPHERE_FIELD_PROCEDURAL_H_
#define VISTA_COMPONENT_WORLD_ATMOSPHERE_FIELD_PROCEDURAL_H_

#include <vector>

#include "vista/component/world/atmosphere/field/field_store.h"
#include "vista/vista_export.h"
#include "vista/terrain/process/land_mask.h"

namespace vista {
namespace atmosphere {

// Shared knobs for procedural atmosphere / ocean field generators.
struct ProceduralOptions {
  unsigned seed = 1;
  float wind_scale = 10.f;
  float wave_hs_scale = 0.55f;
  float cloud_cover_scale = 1.15f;
  float cloud_base_m = 800.f;
  float cloud_thickness_m = 2200.f;
};

// Fills kWindU / kWindV with smooth hash noise on |grid|.
struct VISTA_EXPORT WindNoise {
  ProceduralOptions opts;
  void apply(FieldStore* store, const FieldGrid& grid, int priority) const;
};

// Derives kWaveHs / kWaveDir from wind samples (reads store, then writes).
struct VISTA_EXPORT WaveFromWind {
  ProceduralOptions opts;
  void apply(FieldStore* store, const FieldGrid& grid, int priority) const;
};

// Fills kCloudCover / kCloudBase / kCloudTop with procedural noise.
struct VISTA_EXPORT CloudNoise {
  ProceduralOptions opts;
  void apply(FieldStore* store, const FieldGrid& grid, int priority) const;
};

// Writes kSeaMask (1=sea) as the complement of land rings (sea = !land).
// Empty |land_rings| fail closed to sea=0 (no ocean), not all-sea.
struct VISTA_EXPORT SeaMaskFromLand {
  void apply(FieldStore* store, const FieldGrid& grid,
             const std::vector<LonLatRing>& land_rings, int priority) const;
};

// Reserved shallow-water / advection step. v1 is a no-op.
VISTA_EXPORT void procedural_step(FieldStore* store, double dt_sec);

}  // namespace atmosphere
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_ATMOSPHERE_FIELD_PROCEDURAL_H_
