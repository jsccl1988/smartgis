// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/atmosphere_showcase.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/showcase/atmosphere/session/device_session.h"
#include "app/views/shell/harness/showcase/atmosphere/seed/mode_seed.h"
#include "app/views/shell/harness/showcase/atmosphere/present/present_run.h"
#include "app/views/shell/harness/showcase/atmosphere/common/progress.h"
#include "app/views/shell/harness/showcase/atmosphere/session/session_finish.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"

#include <cstdio>

namespace {
using app::AtmosphereShowcaseMode;
using app::atmosphere_showcase_name;
using app::detail::AtmosphereDeviceSession;
using app::detail::AtmosphereModeSeed;
using app::detail::atmosphere_showcase_mark;
using app::detail::finish_atmosphere_device_session;
using app::detail::prepare_atmosphere_device_session;
using app::detail::run_atmosphere_present;
using app::detail::seed_atmosphere_mode;
using app::detail::verify_atmosphere_mode_flags;

int run_atmosphere_showcase_impl(app::Browser& browser,
                                 AtmosphereShowcaseMode mode) {
  const char* name = atmosphere_showcase_name(mode);
  std::fprintf(stderr, "atmosphere-showcase mode=%s\n", name);
  atmosphere_showcase_mark(name);

  // Owned present HWND is the Scene3D capture target (peer mine/world3d). Skip
  // select_map_tab(2): under GDI-forced showcase sessions, lazy ContentMapView
  // attach on the 3D tab AVs after attach returns (no dump under cdb; mark
  // stuck at mode name). Atmosphere still seeds orbit/passes below.
  atmosphere_showcase_mark("tab3d");
  // Do not pump here: sibling map2d/GDI paint on the shell can AV under
  // concurrent DLL rebuilds (mark never advances past tab3d). Owned present
  // HWND below does not need the shell message queue drained first.
  atmosphere_showcase_mark("pumped");

  AtmosphereDeviceSession session;
  if (const int rc = prepare_atmosphere_device_session(browser, &session)) {
    finish_atmosphere_device_session(browser, &session, /*shutdown_device=*/true);
    return rc;
  }

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    finish_atmosphere_device_session(browser, &session, /*shutdown_device=*/true);
    return 50;
  }

  AtmosphereModeSeed seed;
  if (const int rc = seed_atmosphere_mode(browser, mode, cam, orbit, &seed)) {
    finish_atmosphere_device_session(browser, &session, /*shutdown_device=*/true);
    return rc;
  }
  if (const int rc = verify_atmosphere_mode_flags(mode, cam)) {
    finish_atmosphere_device_session(browser, &session, /*shutdown_device=*/true);
    return rc;
  }

  return run_atmosphere_present(browser, mode, name, &session, cam, orbit,
                                seed);
}

}  // namespace

namespace app {

int atmosphere_showcase_body(Browser& browser, AtmosphereShowcaseMode mode) {
  return run_atmosphere_showcase_impl(browser, mode);
}

int run_atmosphere_showcase(Browser& browser, AtmosphereShowcaseMode mode) {
  const char* suite = nullptr;
  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      suite = "atmosphere.land";
      break;
    case AtmosphereShowcaseMode::kOcean:
      suite = "atmosphere.ocean";
      break;
    case AtmosphereShowcaseMode::kFull:
      suite = "atmosphere.full";
      break;
    case AtmosphereShowcaseMode::kCoast:
      suite = "atmosphere.coast";
      break;
    case AtmosphereShowcaseMode::kLegacy:
      // No interact.dsl twin yet — fall through to C++ body (china DEM parity).
      break;
    case AtmosphereShowcaseMode::kGlobe:
      // C++ body seeds globe + sat cloud; no suite script yet.
      break;
    case AtmosphereShowcaseMode::kNone:
      break;
  }
  if (suite &&
      try_run_suite_script(browser, suite, detail::kAtmosphereShowcaseMarkLeaf,
                           /*clear_marks=*/true)) {
    return 0;
  }
  return atmosphere_showcase_body(browser, mode);
}

}  // namespace app
