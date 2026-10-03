// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/seed/legacy_seed.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/showcase/atmosphere/common/progress.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"

#include <cstdio>
#include <cstdlib>
#include <windows.h>

namespace app {
namespace detail {

int seed_atmosphere_legacy_mode(Browser& browser,
                                content::Scene3dPresenter* cam) {
  if (!cam) {
    return 50;
  }
  atmosphere_showcase_mark("legacy-china-open");
  // Prefer china_city vectors so coast content assert can pass.
  // Under SMT_ATMOSPHERE_SHOWCASE_GPU=1, MapScene::open_path(china_city)
  // AVs (ExitProcess -1 after legacy-open-path). DEM still comes from
  // seed_procedural land rings; BMP label composite runs after present.
  const bool skip_china_open = []() {
    const char* g = std::getenv("SMT_ATMOSPHERE_SHOWCASE_GPU");
    return g && g[0] == '1' && g[1] == '\0';
  }();
  if (!skip_china_open) {
    if (content::MapScene* doc = browser.document()) {
      const bool need_china =
          !doc->has_china_extent() || doc->feature_count() == 0;
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
            atmosphere_showcase_mark("legacy-open-path");
            if (doc->open_path(path_a) && doc->feature_count() > 0) {
              atmosphere_showcase_mark("china-doc-ok");
              break;
            }
            atmosphere_showcase_mark("legacy-open-fail");
          }
        }
      } else {
        atmosphere_showcase_mark("china-doc-reuse");
      }
    }
  } else {
    atmosphere_showcase_mark("china-doc-skip-gpu");
  }
  atmosphere_showcase_mark("legacy-look-begin");
  // Opt-in leftover stereo parity (default product face stays atmosphere).
  apply_china_scene3d_legacy_look(browser);
  atmosphere_showcase_mark("look-legacy");
  if (cam->look_preset() != content::Scene3dLookPreset::kLegacyStereo) {
    std::fprintf(stderr, "atmosphere-showcase: look preset not legacy\n");
    return 53;
  }
  if (cam->gpu().legacy_label_count() < 8) {
    std::fprintf(stderr, "atmosphere-showcase: legacy labels missing\n");
    return 53;
  }
  atmosphere_showcase_mark("labels-ok");
  (void)cam->gpu().ensure_legacy_overlays();
  if (cam->gpu().has_legacy_coast_vectors()) {
    atmosphere_showcase_mark("coast-doc-ok");
  } else {
    atmosphere_showcase_mark("coast-doc-skip");
  }
  return 0;
}

}  // namespace detail
}  // namespace app
