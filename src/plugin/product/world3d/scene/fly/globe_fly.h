// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_FLY_GLOBE_FLY_H_
#define PLUGIN_WORLD3D_SCENE_FLY_GLOBE_FLY_H_

namespace content {
class AtmosphereSession;
class OrbitFrame;
}  // namespace content

namespace vista {
class GlobePass;
}

namespace plugin {

// Cinematic path beats (t01 in [0,1]):
//   space  [0, 0.15)   — deep orbit, sat-cloud limb
//   clouds [0.15, 0.42) — approach through sat-cloud shell
//   high   [0.42, 0.50) — China DEM hold (park / landish score)
//   dive   [0.50, 0.60) — descend onto west DEM entry
//   hug    [0.60, 1]    — horizontal-forward terrain-hug (正前方):
//                         eye above China DEM, look east along path so DEM
//                         relief scrolls under a curved horizon into the
//                         East China Sea
void apply_world3d_globe_flythrough(content::OrbitFrame* orbit,
                                    float t01,
                                    float china_yaw,
                                    float china_pitch,
                                    const vista::GlobePass* globe,
                                    content::AtmosphereSession* session,
                                    bool allow_forward_skim);

// Default: allow horizontal-forward skim on hug beats.
inline void apply_world3d_globe_flythrough(content::OrbitFrame* orbit,
                                           float t01,
                                           float china_yaw,
                                           float china_pitch,
                                           const vista::GlobePass* globe,
                                           content::AtmosphereSession* session) {
  apply_world3d_globe_flythrough(orbit, t01, china_yaw, china_pitch, globe,
                                 session, /*allow_forward_skim=*/true);
}

// Aim yaw/pitch for 105E 35N on the unit globe.
void world3d_china_aim_yaw_pitch(float* yaw, float* pitch);

inline constexpr float kWorld3dGlobeFlySpaceT = 0.06f;
inline constexpr float kWorld3dGlobeFlyCloudsT = 0.28f;
// Terrain-hug over inland China DEM (before coast cyan water).
inline constexpr float kWorld3dGlobeFlyDemT = 0.70f;
// East China coast / nearshore (Gerstner + land in frame).
inline constexpr float kWorld3dGlobeFlyOceanT = 0.86f;
// High-China DEM hold for suite landish score BMP.
inline constexpr float kWorld3dGlobeFlyParkT = 0.48f;

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_FLY_GLOBE_FLY_H_
