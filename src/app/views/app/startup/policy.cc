// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/startup/policy.h"

#include <cstdlib>
#include <cstring>

#include "base/core/log.h"
#include "base/process/switches.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"

namespace app {
namespace {

bool map2d_fps_bench() {
  const char* e = base::switch_cstr("map2d-fps-bench-ms");
  return e && e[0] != '\0' && std::atoi(e) > 0;
}

bool switch_is_zero(const char* name) {
  const char* v = base::switch_cstr(name);
  return v && v[0] == '0' && v[1] == '\0';
}

bool want_flycube_2d() {
  const char* p = base::switch_cstr("prefer-flycube-2d");
  return p && p[0] == '1' && p[1] == '\0';
}

bool pins_content_mapview(const LaunchPolicy& policy) {
  return policy.map2d == Map2dStartup::kPluginScene3d ||
         policy.map2d == Map2dStartup::kProduct ||
         policy.map2d == Map2dStartup::kInteract ||
         policy.map2d == Map2dStartup::kContentGdi;
}

void apply_map2d_face(const LaunchPolicy& policy, bool fps) {
  switch (policy.map2d) {
    case Map2dStartup::kPluginScene3d:
      base::set_switch("force-content-mapview-2d", "1");
      base::set_switch("force-gdi-map-overlay", "1");
      break;
    case Map2dStartup::kInteract:
      base::set_switch("force-content-mapview-2d", "1");
      base::set_switch("prefer-flycube-2d", "0");
      base::set_switch("force-gdi-map-overlay", "0");
      break;
    case Map2dStartup::kContentGdi:
      if (!fps) {
        base::set_switch("force-content-mapview-2d", "1");
        base::set_switch("prefer-flycube-2d", "0");
      }
      break;
    case Map2dStartup::kFlyCube2d:
      base::set_switch("force-content-mapview-2d", "0");
      base::set_switch("prefer-flycube-2d", "1");
      base::set_switch("force-gdi-map-overlay", "0");
      break;
    case Map2dStartup::kProduct:
      if (!fps) {
        base::set_switch("force-content-mapview-2d", "1");
        if (!base::switch_is_one("enable-oop-render")) {
          base::set_switch("disable-oop-render", "1");
        }
      }
      break;
  }
}

void apply_scene3d(const LaunchPolicy& policy) {
  switch (policy.scene3d) {
    case Scene3dStartup::kGdi:
      content::set_scene3d_engine(content::Scene3dEngine::kGdi);
      break;
    case Scene3dStartup::kFlyCube:
      content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
      break;
    case Scene3dStartup::kLeave:
      break;
  }
}

}  // namespace

LaunchPolicy product_startup_policy() {
  LaunchPolicy p;
  p.scene3d = Scene3dStartup::kLeave;
  p.map2d = Map2dStartup::kProduct;
  p.skip_ambox_catalog = false;
  p.force_gdi_overlay = true;
  return p;
}

LaunchPolicy startup_policy_for_scenario(std::string_view scenario_id) {
  if (scenario_id.empty()) {
    return product_startup_policy();
  }
  if (const Scenario* scenario = find_scenario(scenario_id)) {
    return scenario->policy;
  }
  LaunchPolicy fallback;
  fallback.scene3d = Scene3dStartup::kGdi;
  fallback.map2d = Map2dStartup::kContentGdi;
  fallback.skip_ambox_catalog = true;
  fallback.force_gdi_overlay = true;
  return fallback;
}

void apply_startup_policy(const LaunchPolicy& policy_in) {
  LaunchPolicy policy = policy_in;
  switch (policy.scene3d) {
    case Scene3dStartup::kLeave:
    case Scene3dStartup::kGdi:
    case Scene3dStartup::kFlyCube:
      break;
    default:
      policy.scene3d = Scene3dStartup::kGdi;
      policy.map2d = Map2dStartup::kContentGdi;
      policy.skip_ambox_catalog = true;
      policy.force_gdi_overlay = true;
      break;
  }
  switch (policy.map2d) {
    case Map2dStartup::kProduct:
    case Map2dStartup::kContentGdi:
    case Map2dStartup::kFlyCube2d:
    case Map2dStartup::kInteract:
    case Map2dStartup::kPluginScene3d:
      break;
    default:
      policy.map2d = Map2dStartup::kContentGdi;
      break;
  }
  const bool fps = map2d_fps_bench();
  const bool engine_from_env = content::apply_scene3d_engine_from_env();

  // Env SCENE3D_ENGINE must not skip the 2D face: --self-test / ContentGdi
  // still pin ContentMapView even when a leftover scenic/FlyCube env is set.
  // kGdi faces also override leftover FlyCube so self-test has no Display
  // thread (open china_city + DXGI present was crashing after backend-ok).
  if (policy.scene3d == Scene3dStartup::kGdi) {
    apply_scene3d(policy);
  } else if (!engine_from_env) {
    apply_scene3d(policy);
  }
  apply_map2d_face(policy, fps);
  LOGGING(LOG_INFO,
          "startup: policy applied scene3d=%d map2d=%d force-content-mapview-2d=%s "
          "prefer-flycube-2d=%s",
          static_cast<int>(policy.scene3d), static_cast<int>(policy.map2d),
          base::switch_cstr("force-content-mapview-2d")
              ? base::switch_cstr("force-content-mapview-2d")
              : "(null)",
          base::switch_cstr("prefer-flycube-2d")
              ? base::switch_cstr("prefer-flycube-2d")
              : "(null)");

  // Stereo / explicit 2D FlyCube / FPS bench: allow DXGI on Map Edit.
  // Default Scene3d FlyCube must not clear the 2D ContentMapView gate on
  // product / interact / plugin-3d (SetWindowPos deadlock).
  if (!pins_content_mapview(policy) &&
      (content::prefer_scene3d_stereo_gl() || want_flycube_2d() || fps ||
       policy.map2d == Map2dStartup::kFlyCube2d)) {
    base::set_switch("force-content-mapview-2d", "0");
  }

  if (content::prefer_map2d_scenic() || content::prefer_scene3d_scenic()) {
    if (!pins_content_mapview(policy)) {
      base::set_switch("force-content-mapview-2d", "0");
      base::set_switch("prefer-flycube-2d", "0");
    }
    if (content::prefer_map2d_scenic()) {
      base::set_switch("map2d-engine", "scenic");
    }
    if (content::prefer_scene3d_scenic()) {
      base::set_switch("scene3d-engine", "scenic");
    }
  }

  if (!fps && policy.force_gdi_overlay &&
      !switch_is_zero("force-gdi-map-overlay")) {
    base::set_switch("force-gdi-map-overlay", "1");
  }
  if (policy.skip_ambox_catalog) {
    base::set_switch("skip-ambox-catalog", "1");
  }
}

}  // namespace app
