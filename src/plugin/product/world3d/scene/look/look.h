// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_LOOK_LOOK_H_
#define PLUGIN_WORLD3D_SCENE_LOOK_LOOK_H_

#include <string_view>

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace plugin {

// Product True-Earth looks formerly owned by --atmosphere-showcase modes.
enum class World3dLook {
  kLand,
  kOcean,
  kFull,
  kCoast,
  kGlobe,
  kLegacy,
  kEastChina,
};

// Outputs from seeding orbit / globe fly for a look.
struct World3dLookSeed {
  float globe_china_yaw = 0.f;
  float globe_china_pitch = 0.f;
  bool globe_flythrough = false;
};

const char* world3d_look_name(World3dLook look);
bool parse_world3d_look(std::string_view id, World3dLook* out);

// Seeds orbit + atmosphere flags. Returns false if cam/orbit missing.
bool apply_world3d_look(content::Scene3dPresenter* cam,
                        content::OrbitFrame* orbit,
                        World3dLook look,
                        World3dLookSeed* out);

// Contract check used by harness gates. Logs mismatches to stderr.
bool verify_world3d_look(World3dLook look, content::Scene3dPresenter* cam);

// Planar East-China product face: kEastChina look, optional perf-bare strip
// (sky/cloud/fog off), else contour suite. Returns false if look apply fails.
bool apply_world3d_east_china_face(content::Scene3dPresenter* cam,
                                   content::OrbitFrame* orbit,
                                   bool perf_bare);

// True-Earth globe face: kGlobe look + contour suite + flythrough park at
// t=0.42. Returns false if look apply fails.
bool apply_world3d_true_earth_globe(content::Scene3dPresenter* cam,
                                    content::OrbitFrame* orbit,
                                    World3dLookSeed* out);

// After kLegacy look: ensure CPU label overlays and verify label count.
// Returns false (and logs) when the legacy overlay contract fails.
bool ensure_world3d_legacy_overlays(content::Scene3dPresenter* cam);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_LOOK_LOOK_H_
