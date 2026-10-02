// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/atmosphere_showcase.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/showcase/atmosphere/device_session.h"
#include "app/views/shell/harness/showcase/atmosphere/mode_seed.h"
#include "app/views/shell/harness/showcase/atmosphere/present_run.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"

#include <cstdio>
#include <windows.h>

namespace {

using app::AtmosphereShowcaseMode;
using app::atmosphere_showcase_name;
using app::detail::AtmosphereDeviceSession;
using app::detail::AtmosphereModeSeed;
using app::detail::detach_maps;
using app::detail::kAtmosphereShowcaseMarkLeaf;
using app::detail::prepare_atmosphere_device_session;
using app::detail::run_atmosphere_present;
using app::detail::seed_atmosphere_mode;
using app::detail::verify_atmosphere_mode_flags;
using app::detail::write_mark;

void showcase_mark(const char* step) {
  write_mark(kAtmosphereShowcaseMarkLeaf, step, /*truncate=*/false);
}

void cleanup_device_session(app::Browser& browser,
                            AtmosphereDeviceSession* session,
                            bool shutdown_device) {
  if (!session) {
    detach_maps(browser);
    return;
  }
  if (shutdown_device && session->device) {
    session->device->shutdown();
  }
  if (session->owned_present_hwnd) {
    DestroyWindow(session->owned_present_hwnd);
    session->owned_present_hwnd = nullptr;
  }
  detach_maps(browser);
}

int run_atmosphere_showcase_impl(app::Browser& browser,
                                 AtmosphereShowcaseMode mode) {
  const char* name = atmosphere_showcase_name(mode);
  std::fprintf(stderr, "atmosphere-showcase mode=%s\n", name);
  showcase_mark(name);

  // Owned present HWND is the Scene3D capture target (peer mine/world3d). Skip
  // select_map_tab(2): under GDI-forced showcase sessions, lazy ContentMapView
  // attach on the 3D tab AVs after attach returns (no dump under cdb; mark
  // stuck at mode name). Atmosphere still seeds orbit/passes below.
  showcase_mark("tab3d");
  // Do not pump here: sibling map2d/GDI paint on the shell can AV under
  // concurrent DLL rebuilds (mark never advances past tab3d). Owned present
  // HWND below does not need the shell message queue drained first.
  showcase_mark("pumped");

  AtmosphereDeviceSession session;
  if (const int rc = prepare_atmosphere_device_session(browser, &session)) {
    cleanup_device_session(browser, &session, /*shutdown_device=*/true);
    return rc;
  }

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    cleanup_device_session(browser, &session, /*shutdown_device=*/true);
    return 50;
  }

  AtmosphereModeSeed seed;
  if (const int rc = seed_atmosphere_mode(browser, mode, cam, orbit, &seed)) {
    cleanup_device_session(browser, &session, /*shutdown_device=*/true);
    return rc;
  }
  if (const int rc = verify_atmosphere_mode_flags(mode, cam)) {
    cleanup_device_session(browser, &session, /*shutdown_device=*/true);
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
