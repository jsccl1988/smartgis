// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_STARTUP_ATMOSPHERE_SHOWCASE_RUN_H_
#define APP_VIEWS_APP_STARTUP_ATMOSPHERE_SHOWCASE_RUN_H_

namespace app {

class Browser;

// C++ fallback for browser.world3d.legacy when the suite script cannot run
// (Debug has no CPython embed). Applies kLegacyStereo look, exports the
// atmosphere-showcase-legacy BMP (and timed present JSON when PRESENT_COUNT
// is set), then writes harness marks.
int run_atmosphere_legacy_showcase(Browser& browser);

// C++ fallback for browser.world3d.full: land → ocean → coast → full BMPs
// and the suite required_marks (bmp-*-ok / bmp-ok / orbit tokens / pass).
int run_atmosphere_full_showcase(Browser& browser);

}  // namespace app

#endif  // APP_VIEWS_APP_STARTUP_ATMOSPHERE_SHOWCASE_RUN_H_
