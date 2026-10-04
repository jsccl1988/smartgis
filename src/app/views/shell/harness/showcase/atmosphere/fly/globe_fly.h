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

// Cinematic Google-Earth / product-splash orbit path (space → high → Tibet skim).
// |t01| in [0,1]; |china_yaw|/|china_pitch| seed the China aim before dive.
void apply_globe_flythrough(content::OrbitFrame* orbit,
                            float t01,
                            float china_yaw,
                            float china_pitch,
                            const vista::GlobePass* globe,
                            content::AtmosphereSession* session);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_FLY_GLOBE_FLY_H_
