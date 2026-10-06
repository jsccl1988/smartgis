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
//   space  [0, 0.15)  — deep orbit, sat-cloud limb
//   clouds [0.15, 0.42) — approach through sat-cloud shell
//   DEM    [0.42, 0.85) — surface dive + west→east skim
//   ocean  [0.85, 1]  — East China Sea Gerstner
void apply_world3d_globe_flythrough(content::OrbitFrame* orbit,
                                    float t01,
                                    float china_yaw,
                                    float china_pitch,
                                    const vista::GlobePass* globe,
                                    content::AtmosphereSession* session);

// Aim yaw/pitch for 105E 35N on the unit globe.
void world3d_china_aim_yaw_pitch(float* yaw, float* pitch);

inline constexpr float kWorld3dGlobeFlySpaceT = 0.06f;
inline constexpr float kWorld3dGlobeFlyCloudsT = 0.28f;
inline constexpr float kWorld3dGlobeFlyDemT = 0.76f;
inline constexpr float kWorld3dGlobeFlyOceanT = 1.0f;
inline constexpr float kWorld3dGlobeFlyParkT = 0.48f;

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_FLY_GLOBE_FLY_H_
