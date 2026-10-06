// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_ATMOSPHERE_SEED_LEGACY_SEED_H_
#define APP_VIEWS_HARNESS_SHOWCASE_ATMOSPHERE_SEED_LEGACY_SEED_H_

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace app {

class Browser;

namespace detail {

// Seeds leftover stereo look + china_city open for --atmosphere-showcase=legacy.
// Returns 0 on success, else an exit code (typically 53).
int seed_atmosphere_legacy_mode(Browser& browser,
                                content::Scene3dPresenter* cam);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_ATMOSPHERE_SEED_LEGACY_SEED_H_
