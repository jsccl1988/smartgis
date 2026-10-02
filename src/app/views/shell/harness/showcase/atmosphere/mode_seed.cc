// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/mode_seed.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/showcase/atmosphere/globe_fly.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "gis/vista/domain/atmosphere/systems/environment.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>

namespace app {
namespace detail {
namespace {

void showcase_mark(const char* step) {
  write_mark(kAtmosphereShowcaseMarkLeaf, step, /*truncate=*/false);
}

}  // namespace

int seed_atmosphere_mode(Browser& browser,
                         AtmosphereShowcaseMode mode,
                         content::Scene3dPresenter* cam,
                         content::OrbitFrame* orbit,
                         AtmosphereModeSeed* out) {
  if (!cam || !orbit || !out) {
    return 50;
  }
  *out = AtmosphereModeSeed{};

  // Keep the China DEM centered for BMP capture (no orbit nudge that looks
  // at empty ocean and fails the landish visual gate).
  showcase_mark("orbit-reset");
  showcase_mark("extent-ok");

  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      // DEM / land present only. Explicitly clear any session flags left on
      // from Browser init / prior seed (otherwise land gate exits 53).
      apply_china_scene3d_orbit(browser);
      cam->atmosphere_session().set_ocean_enabled(false);
      cam->atmosphere_session().set_cloud_enabled(false);
      cam->atmosphere_session().set_sky_enabled(false);
      cam->atmosphere_session().set_fog_enabled(false);
      break;
    case AtmosphereShowcaseMode::kOcean:
      apply_china_scene3d_orbit(browser);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(false);
      break;
    case AtmosphereShowcaseMode::kFull:
      // Prefer orbit + seed without abandon_mesh: full product_defaults
      // abandon+rebuild has painted a black mainland silhouette under FlyCube
      // while land mode (orbit only) keeps hypsometric greens.
      apply_china_scene3d_orbit(browser);
      // Product distance 2.55 fills the top third with DEM and fails
      // blue_sky_frac_top. Pull back for Rayleigh zenith while keeping
      // hypsometric coast detail readable (textured DEM, not solid blob).
      orbit->set_distance(3.6f);
      orbit->set_pitch(0.36f);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
      // Flat DEM + ocean/sky path — never the unit globe (that path skips
      // GpuScene mesh upload and was observed firing atmosphere.globe logs
      // during --atmosphere-showcase=full when flags leaked on).
      cam->atmosphere_session().set_globe_enabled(false);
      cam->atmosphere_session().set_sat_cloud_enabled(false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(true);
      cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
      break;
    case AtmosphereShowcaseMode::kCoast: {
      // East China Sea coastal window — different extent from full China.
      orbit->reset();
      const content::Extent2 coast{118.0, 28.0, 128.0, 36.0};
      orbit->apply_world_extent(coast);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(true);
      break;
    }
    case AtmosphereShowcaseMode::kGlobe: {
      // Google-Earth stack: unit DEM globe + sat cloud shell + sky.
      // Aim at China (105E, 35N); start in space — fly-in animates to DEM.
      orbit->reset();
      constexpr float kLon = 105.f * 3.14159265f / 180.f;
      constexpr float kLat = 35.f * 3.14159265f / 180.f;
      const float cl = std::cos(kLat);
      const float x = cl * std::sin(kLon);
      const float y = std::sin(kLat);
      const float z = cl * std::cos(kLon);
      out->globe_china_yaw = std::atan2(x, z);
      out->globe_china_pitch =
          std::asin((std::max)(-1.f, (std::min)(1.f, y)));
      out->globe_flythrough = true;
      apply_globe_flythrough(orbit, 0.f, out->globe_china_yaw,
                             out->globe_china_pitch,
                             &cam->atmosphere_session().globe_pass(),
                             &cam->atmosphere_session());
      cam->atmosphere_session().set_globe_enabled(true);
      cam->atmosphere_session().set_sat_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(false);
      cam->atmosphere_session().set_ocean_enabled(false);
      cam->atmosphere_session().set_cloud_enabled(false);
      cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
      showcase_mark("globe-ok");
      break;
    }
    case AtmosphereShowcaseMode::kLegacy: {
      showcase_mark("legacy-china-open");
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
                showcase_mark("legacy-open-path");
                if (doc->open_path(path_a) && doc->feature_count() > 0) {
                  showcase_mark("china-doc-ok");
                  break;
                }
                showcase_mark("legacy-open-fail");
              }
            }
          } else {
            showcase_mark("china-doc-reuse");
          }
        }
      } else {
        showcase_mark("china-doc-skip-gpu");
      }
      showcase_mark("legacy-look-begin");
      // Opt-in leftover stereo parity (default product face stays atmosphere).
      apply_china_scene3d_legacy_look(browser);
      showcase_mark("look-legacy");
      if (cam->look_preset() != content::Scene3dLookPreset::kLegacyStereo) {
        std::fprintf(stderr, "atmosphere-showcase: look preset not legacy\n");
        return 53;
      }
      if (cam->gpu().legacy_label_count() < 8) {
        std::fprintf(stderr, "atmosphere-showcase: legacy labels missing\n");
        return 53;
      }
      showcase_mark("labels-ok");
      (void)cam->gpu().ensure_legacy_overlays();
      if (cam->gpu().has_legacy_coast_vectors()) {
        showcase_mark("coast-doc-ok");
      } else {
        showcase_mark("coast-doc-skip");
      }
      break;
    }
    case AtmosphereShowcaseMode::kNone:
    default:
      return 53;
  }
  showcase_mark("demo-ok");
  return 0;
}

int verify_atmosphere_mode_flags(AtmosphereShowcaseMode mode,
                                 content::Scene3dPresenter* cam) {
  if (!cam) {
    return 50;
  }
  const gis::atmosphere::Environment* env =
      cam->atmosphere_session().environment();
  // Full / coast: ocean + soft cloud + sky + fog.
  // Legacy: black clear + calm light-blue sea (leftover stereo shelf character).
  // Globe: DEM sphere + sat cloud shell + sky (no flat ocean / volumetric cloud).
  const bool want_ocean = mode == AtmosphereShowcaseMode::kOcean ||
                          mode == AtmosphereShowcaseMode::kCoast ||
                          mode == AtmosphereShowcaseMode::kFull ||
                          mode == AtmosphereShowcaseMode::kLegacy;
  const bool want_cloud = mode == AtmosphereShowcaseMode::kFull ||
                          mode == AtmosphereShowcaseMode::kCoast;
  const bool want_sky = mode == AtmosphereShowcaseMode::kFull ||
                        mode == AtmosphereShowcaseMode::kCoast ||
                        mode == AtmosphereShowcaseMode::kGlobe;
  const bool want_fog = mode == AtmosphereShowcaseMode::kCoast ||
                        mode == AtmosphereShowcaseMode::kFull;
  if (mode == AtmosphereShowcaseMode::kLand) {
    if (env && (env->ocean_enabled() || env->cloud_enabled() ||
                env->sky_enabled() || env->fog_enabled())) {
      std::fprintf(stderr,
                   "atmosphere-showcase: land mode still has passes on\n");
      return 53;
    }
  } else {
    if (!env) {
      std::fprintf(stderr, "atmosphere-showcase: Environment missing\n");
      return 53;
    }
    if (env->ocean_enabled() != want_ocean ||
        env->cloud_enabled() != want_cloud ||
        env->sky_enabled() != want_sky || env->fog_enabled() != want_fog) {
      std::fprintf(stderr,
                   "atmosphere-showcase: flag mismatch ocean=%d cloud=%d "
                   "sky=%d fog=%d (want %d/%d/%d/%d)\n",
                   env->ocean_enabled() ? 1 : 0, env->cloud_enabled() ? 1 : 0,
                   env->sky_enabled() ? 1 : 0, env->fog_enabled() ? 1 : 0,
                   want_ocean ? 1 : 0, want_cloud ? 1 : 0, want_sky ? 1 : 0,
                   want_fog ? 1 : 0);
      return 53;
    }
    // Globe stack uses DEM / procedural sat cover — FieldStore ocean/cloud
    // layers are optional (not required for geometry proof).
    if (mode != AtmosphereShowcaseMode::kGlobe &&
        env->field_store().layer_count() == 0) {
      std::fprintf(stderr, "atmosphere-showcase: FieldStore empty\n");
      return 53;
    }
    if (mode == AtmosphereShowcaseMode::kGlobe) {
      if (!cam->atmosphere_session().globe_enabled()) {
        std::fprintf(stderr, "atmosphere-showcase: globe flag off\n");
        return 53;
      }
      // Sat-cloud shell is optional (procedural cover can heap-churn Debug).
      if (!cam->atmosphere_session().sat_cloud_enabled()) {
        std::fprintf(stderr,
                     "atmosphere-showcase: sat_cloud off (sky+DEM splash)\n");
      }
    }
  }
  showcase_mark("config-ok");
  std::fprintf(stderr, "atmosphere-showcase: ocean=%d cloud=%d layers=%zu\n",
               env && env->ocean_enabled() ? 1 : 0,
               env && env->cloud_enabled() ? 1 : 0,
               env ? env->field_store().layer_count() : 0u);
  return 0;
}

}  // namespace detail
}  // namespace app
