// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/seed/legacy_seed.h"

#include <cstdio>
#include <windows.h>

#include "base/process/switches.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/product/world3d/scene/look/look.h"
#include "plugin/runtime/host/capability/shell.h"

namespace plugin {
namespace detail {

int seed_atmosphere_legacy_mode(HarnessShell& browser,
                                content::Scene3dPresenter* cam) {
  if (!cam) {
    return 50;
  }
  atmosphere_mark("legacy-china-open");
  // Prefer china_city vectors so coast content assert can pass.
  if (content::GisScene* doc = browser.document()) {
    const bool need_china =
        !doc->has_land_surface() || doc->feature_count() == 0;
    if (need_china) {
      wchar_t exe_dir[MAX_PATH] = {};
      if (GetModuleFileNameW(nullptr, exe_dir, MAX_PATH) > 0) {
        wchar_t* slash = wcsrchr(exe_dir, L'\\');
        if (slash) {
          slash[1] = L'\0';
        }
        const wchar_t* cands[] = {L"..\\data\\china_city.gpkg",
                                  L"..\\data\\china_city.geojson",
                                  L"data\\china_city.gpkg"};
        for (const wchar_t* rel : cands) {
          wchar_t path_w[MAX_PATH] = {};
          if (wcscpy_s(path_w, exe_dir) != 0 ||
              wcscat_s(path_w, rel) != 0) {
            continue;
          }
          if (GetFileAttributesW(path_w) == INVALID_FILE_ATTRIBUTES) {
            continue;
          }
          char path_a[MAX_PATH] = {};
          WideCharToMultiByte(CP_UTF8, 0, path_w, -1, path_a, MAX_PATH,
                              nullptr, nullptr);
          atmosphere_mark("legacy-open-path");
          if (doc->open_path(path_a) && doc->feature_count() > 0) {
            atmosphere_mark("china-doc-ok");
            break;
          }
          atmosphere_mark("legacy-open-fail");
        }
      }
    } else {
      atmosphere_mark("china-doc-reuse");
    }
  }
  atmosphere_mark("legacy-look-begin");
  plugin::World3dLookSeed seed;
  if (!plugin::apply_world3d_look(cam, browser.orbit_frame(),
                                 plugin::World3dLook::kLegacy, &seed)) {
    return 50;
  }
  atmosphere_mark("look-legacy");
  if (!plugin::ensure_world3d_legacy_overlays(cam)) {
    return 53;
  }
  atmosphere_mark("labels-ok");
  // Out-of-line Presenter accessor (avoid inline cam->gpu() sizeof skew).
  if (cam->has_legacy_coast_vectors()) {
    atmosphere_mark("coast-doc-ok");
  } else {
    atmosphere_mark("coast-doc-skip");
  }
  return 0;
}

}  // namespace detail
}  // namespace plugin
