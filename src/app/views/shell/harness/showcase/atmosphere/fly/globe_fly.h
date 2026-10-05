// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_FLY_GLOBE_FLY_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_FLY_GLOBE_FLY_H_

namespace content {
class AtmosphereSession;
class OrbitFrame;
}  // namespace content

namespace vista {
class GlobePass;
}

namespace app {
namespace detail {

// Cinematic path beats (t01 in [0,1]):
//   space  [0, 0.15)  — deep orbit, sat-cloud limb
//   clouds [0.15, 0.42) — approach through sat-cloud shell
//   DEM    [0.42, 0.85) — surface dive + west→east skim
//   ocean  [0.85, 1]  — East China Sea Gerstner
void apply_globe_flythrough(content::OrbitFrame* orbit,
                            float t01,
                            float china_yaw,
                            float china_pitch,
                            const vista::GlobePass* globe,
                            content::AtmosphereSession* session);

// Named stage centers for keyframe captures / marks.
inline constexpr float kGlobeFlySpaceT = 0.06f;
inline constexpr float kGlobeFlyCloudsT = 0.28f;
inline constexpr float kGlobeFlyDemT = 0.76f;
inline constexpr float kGlobeFlyOceanT = 1.0f;
inline constexpr float kGlobeFlyParkT = 0.48f;

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_FLY_GLOBE_FLY_H_
