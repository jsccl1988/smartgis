// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_MODE_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_MODE_H_

namespace plugin {

// Atmosphere harness modes for world3d scenario commands
// (`world3d.scenario.atmosphere.*`). Selected via plugin.json
// `startup.scenario` / suite id — not Views CLI.
enum class AtmosphereShowcaseMode {
  kNone,
  kLand,
  kOcean,
  kFull,
  kCoast,
  // Leftover stereo look on Views Scene3D (black clear + hypsometric + labels).
  kLegacy,
  // Google-Earth-like: DEM globe + sat cloud shell + sky atmosphere.
  kGlobe,
};

inline const char* atmosphere_showcase_name(AtmosphereShowcaseMode mode) {
  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      return "land";
    case AtmosphereShowcaseMode::kOcean:
      return "ocean";
    case AtmosphereShowcaseMode::kFull:
      return "full";
    case AtmosphereShowcaseMode::kCoast:
      return "coast";
    case AtmosphereShowcaseMode::kLegacy:
      return "legacy";
    case AtmosphereShowcaseMode::kGlobe:
      return "globe";
    case AtmosphereShowcaseMode::kNone:
    default:
      return "none";
  }
}

}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_MODE_H_
