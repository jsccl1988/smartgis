// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/camera/camera_fly.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "plugin/product/world3d/scene/fly/globe_fly.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {
namespace detail {
namespace {

constexpr float kPi = 3.14159265f;

float ease_smooth(float t) {
  t = (std::max)(0.f, (std::min)(1.f, t));
  return t * t * (3.f - 2.f * t);
}

void invalidate_scene(Browser& browser) {
  if (ui::views::DrawHost* pane = browser.scene_draw_host()) {
    pane->invalidate_native();
  }
}

bool fly_orbit(Browser& browser, content::OrbitFrame* orbit, int ms,
               int steps) {
  const float yaw0 = orbit->yaw();
  const float pitch0 = orbit->pitch();
  const float dist0 = orbit->distance();
  // ~270° arc so the world component is seen from multiple sides.
  constexpr float kArc = kPi * 1.5f;
  const int n = (std::max)(steps, 4);
  const int step_ms = (std::max)(1, ms / n);
  for (int i = 0; i <= n; ++i) {
    const float t = ease_smooth(static_cast<float>(i) / static_cast<float>(n));
    orbit->set_yaw(yaw0 + kArc * t);
    // Gentle pitch bob so relief / volume roofs catch light.
    orbit->set_pitch(pitch0 + 0.08f * std::sin(t * kPi));
    orbit->set_distance(dist0);
    invalidate_scene(browser);
    pump_messages(static_cast<DWORD>(step_ms));
  }
  // Park on a readable 3/4 pose (not the start).
  orbit->set_yaw(yaw0 + kArc * 0.35f);
  orbit->set_pitch(pitch0);
  orbit->set_distance(dist0);
  invalidate_scene(browser);
  pump_messages(80);
  return true;
}

bool fly_spherical(Browser& browser, content::OrbitFrame* orbit, int ms,
                   int steps) {
  content::Scene3dPresenter* cam = browser.scene3d();
  if (!cam) {
    return false;
  }

  // Globe showcases: reuse the product cinematic path (space→skim→ocean).
  if (cam->atmosphere_session().globe_enabled()) {
    float china_yaw = 0.f;
    float china_pitch = 0.f;
    plugin::world3d_china_aim_yaw_pitch(&china_yaw, &china_pitch);
    const int n = (std::max)(steps, 6);
    const int step_ms = (std::max)(1, ms / n);
    for (int i = 0; i <= n; ++i) {
      const float t01 =
          ease_smooth(static_cast<float>(i) / static_cast<float>(n));
      plugin::apply_world3d_globe_flythrough(
          orbit, t01, china_yaw, china_pitch,
          &cam->atmosphere_session().globe_pass(), &cam->atmosphere_session());
      invalidate_scene(browser);
      pump_messages(static_cast<DWORD>(step_ms));
    }
    plugin::apply_world3d_globe_flythrough(
        orbit, plugin::kWorld3dGlobeFlyParkT, china_yaw, china_pitch,
        &cam->atmosphere_session().globe_pass(), &cam->atmosphere_session());
    invalidate_scene(browser);
    pump_messages(80);
    return true;
  }

  // Regional / DEM / overlay: spherical approach (far → framed) then yaw skim.
  const float yaw0 = orbit->yaw();
  const float pitch0 = orbit->pitch();
  const float dist_near = orbit->distance();
  const float dist_far =
      (std::min)(12.f, (std::max)(dist_near * 3.2f, dist_near + 2.5f));
  const int n = (std::max)(steps, 6);
  const int step_ms = (std::max)(1, ms / n);
  for (int i = 0; i <= n; ++i) {
    const float t = ease_smooth(static_cast<float>(i) / static_cast<float>(n));
    float dist = dist_far;
    float yaw = yaw0;
    float pitch = pitch0;
    if (t < 0.45f) {
      const float u = t / 0.45f;
      dist = dist_far + (dist_near - dist_far) * u;
      yaw = yaw0 + 0.35f * u;
      pitch = pitch0 * (1.f - 0.25f * u);
    } else {
      const float u = (t - 0.45f) / 0.55f;
      dist = dist_near;
      yaw = yaw0 + 0.35f + u * kPi * 1.15f;
      pitch = pitch0 * 0.75f + 0.12f * std::sin(u * kPi);
    }
    orbit->set_distance(dist);
    orbit->set_yaw(yaw);
    orbit->set_pitch((std::max)(0.18f, (std::min)(0.85f, pitch)));
    invalidate_scene(browser);
    pump_messages(static_cast<DWORD>(step_ms));
  }
  orbit->set_distance(dist_near);
  orbit->set_yaw(yaw0 + 0.55f);
  orbit->set_pitch(pitch0);
  invalidate_scene(browser);
  pump_messages(80);
  return true;
}

}  // namespace

bool camera_fly(Browser& browser, const std::string& mode, int ms, int steps) {
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!orbit) {
    return false;
  }
  const int wall_ms = (std::max)(ms, 200);
  const int n = (std::max)(steps, 4);
  if (mode == "spherical" || mode == "globe" || mode == "sphere") {
    return fly_spherical(browser, orbit, wall_ms, n);
  }
  // Default: orbit tour around the framed component.
  return fly_orbit(browser, orbit, wall_ms, n);
}

}  // namespace detail
}  // namespace app
