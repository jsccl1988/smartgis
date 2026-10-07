// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/seed/world3d_seed.h"

#include <cstddef>
#include <cstdio>

#include <windows.h>

#include "base/process/switches.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/product/world3d/scene/look/look.h"
#include "plugin/runtime/host/capability/shell.h"

namespace plugin {
namespace detail {

bool resolve_world3d_pointcloud_sample(char* out_utf8, size_t out_cap) {
  const wchar_t* colored[] = {L"..\\data\\pointcloud_public_sample.txt",
                              L"data\\pointcloud_public_sample.txt"};
  if (resolve_rel_under_exe(colored, 2, out_utf8, out_cap)) {
    return true;
  }
  const wchar_t* las[] = {L"..\\data\\plugin\\world3d_pointcloud_sample.las",
                          L"data\\plugin\\world3d_pointcloud_sample.las"};
  return resolve_rel_under_exe(las, 2, out_utf8, out_cap);
}

bool world3d_perf_bare_enabled() {
  const char* e = base::switch_cstr("plugin-world3d-perf-bare");
  return e && e[0] == '1' && e[1] == '\0';
}

void seed_world3d_earth_atmosphere(HarnessShell& browser,
                                   content::Scene3dPresenter* cam) {
  if (!cam) {
    return;
  }
  plugin_mark("seed-china-begin");
  const bool bare = world3d_perf_bare_enabled();
  if (!plugin::apply_world3d_east_china_face(cam, browser.orbit_frame(),
                                             bare)) {
    return;
  }
  plugin_mark("seed-china-defaults");
  if (bare) {
    plugin_mark("perf-bare");
    std::fprintf(stderr,
                 "plugin-showcase: world3d perf-bare "
                 "(sky/ocean/cloud/fog off)\n");
  } else {
    plugin_mark("seed-contour");
    plugin_mark("earth-atmo");
  }
  plugin_mark("seed-china-flags");
  plugin_mark("orbit-china");
}

void seed_world3d_true_earth_globe(HarnessShell& browser,
                                   content::Scene3dPresenter* cam) {
  if (!cam) {
    return;
  }
  plugin_mark("seed-globe-begin");
  plugin::World3dLookSeed seed;
  if (!plugin::apply_world3d_true_earth_globe(cam, browser.orbit_frame(),
                                              &seed)) {
    return;
  }
  plugin_mark("seed-contour");
  plugin_mark("seed-globe-flags");
  plugin_mark("earth-atmo");
  plugin_mark("orbit-globe");
}

}  // namespace detail
}  // namespace plugin
