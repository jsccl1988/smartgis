// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/gis_host_extent.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/scene3d/frame/prep/scene3d_prep_mailbox.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "vista/terrain/dem/dem_frame.h"
#include "render/rhi/rhi.h"
#include "tool/nav/camera_nav.h"
#include "tool/draft/draft.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>
#include "base/process/switches.h"

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void disable_crt_assert_dialog() {
#if defined(_MSC_VER) && defined(_DEBUG)
  // /RTC stack checks and CRT asserts otherwise MessageBox + WaitMessage,
  // hanging non-interactive te / exe_smoke runners forever.
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
}

}  // namespace

int main() {
  disable_crt_assert_dialog();
  // Default FlyCube; switch via set_scene3d_engine or SCENE3D_ENGINE.
  {
    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
    expect(content::prefer_scene3d_flycube(), "default prefer FlyCube RHI");
    expect(!content::force_content_mapview_3d(), "default not force content");
    expect(!content::prefer_scene3d_stereo_gl(), "default not stereo");
    expect(!content::prefer_scene3d_gdi(), "default not GDI");

    content::set_scene3d_engine(content::Scene3dEngine::kStereoGl);
    expect(content::prefer_scene3d_stereo_gl(), "stereo selected");
    expect(!content::prefer_scene3d_flycube(), "stereo disables FlyCube");
    expect(content::force_content_mapview_3d(), "stereo forces content path");

    content::set_scene3d_engine(content::Scene3dEngine::kGdi);
    expect(content::prefer_scene3d_gdi(), "GDI selected");
    expect(!content::prefer_scene3d_flycube(), "GDI disables FlyCube");
    // GDI paints Scene3dPresenter on the product HWND; only leftover stereo
    // still forces ContentMapView (navy SharedSurface flash otherwise).
    expect(!content::force_content_mapview_3d(),
           "GDI does not force ContentMapView");

    content::set_scene3d_engine(content::Scene3dEngine::kScenic);
    expect(content::prefer_scene3d_scenic(), "Scenic selected");
    expect(!content::prefer_scene3d_flycube(), "Scenic disables FlyCube");
    expect(!content::force_content_mapview_3d(),
           "Scenic does not force leftover HWND");
    {
      auto scenic_cam = std::make_unique<content::Scene3dPresenter>();
      expect(scenic_cam->hosts_scenic_present(),
             "Scenic env/API hosts scenic.dll");
    }

    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
    expect(content::prefer_scene3d_flycube(), "restore FlyCube");
    expect(content::scene3d_engine() == content::Scene3dEngine::kFlyCube,
           "engine getter matches FlyCube");
  }

  // Harness env: SCENE3D_ENGINE selects leftover GL vs D3D under kStereoGl.
  {
    base::set_switch("scene3d-engine", "stereo_gl");
    expect(content::apply_scene3d_engine_from_env(), "env stereo_gl applies");
    expect(content::prefer_scene3d_stereo_gl(), "env stereo_gl engine");
    expect(content::prefer_scene3d_stereo_opengl(), "env stereo_gl -OpenGL");
    expect(!content::prefer_scene3d_stereo_d3d(), "env stereo_gl not D3D");

    base::set_switch("scene3d-engine", "stereo_d3d");
    expect(content::apply_scene3d_engine_from_env(), "env stereo_d3d applies");
    expect(content::prefer_scene3d_stereo_gl(), "env stereo_d3d still stereo");
    expect(content::prefer_scene3d_stereo_d3d(), "env stereo_d3d -D3D");
    expect(!content::prefer_scene3d_stereo_opengl(), "env stereo_d3d not GL");

    base::set_switch("scene3d-engine", "scenic");
    expect(content::apply_scene3d_engine_from_env(), "env scenic applies");
    expect(content::prefer_scene3d_scenic(), "env scenic");

    base::set_switch("scene3d-engine", "flycube");
    expect(content::apply_scene3d_engine_from_env(), "env flycube applies");
    expect(content::prefer_scene3d_flycube(), "env flycube");
    base::set_switch("scene3d-engine", "");
    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
  }

  // Heap-allocate OrbitFrame + Scene3dPresenter: Debug /RTC treats a stack
  // Scene3dPresenter (WorldPass / atmosphere bag) as adjacent smash into
  // orbit locals (wind_orbit / dem_orbit / orbit).
  auto orbit = std::make_unique<content::OrbitFrame>();
  auto cam = std::make_unique<content::Scene3dPresenter>();
  cam->bind_orbit(orbit.get());
  expect(!cam->hosts_shared_scene(), "fresh presenter has no GisContents");
  expect(std::fabs(content::kScene3dDefaultYaw - vista::kDemDefaultOrbitYaw) < 1e-6f,
         "host yaw aliases gis shared constant");
  expect(std::fabs(orbit->yaw() - vista::kDemDefaultOrbitYaw) < 1e-4f,
         "default yaw south-of-target");
  // make_orbit_camera: ez = dist * cos(pitch) * cos(yaw). South-of-target
  // requires ez < 0 so geographic +Z (north) sits toward the screen top.
  {
    const float ez = orbit->distance() * std::cos(orbit->pitch()) *
                     std::cos(orbit->yaw());
    expect(ez < 0.f, "default eye south of origin (north-up)");
  }
  // RH lookAt looking +Z -camera right = -X; mesh X=-lon puts east on right.
  expect(vista::dem_lon_to_x(121.0) < vista::dem_lon_to_x(88.0),
         "east X < west X (screen-right looking north)");
  expect(orbit->camera_matrices(1.333f).kind ==
             render::rhi::CameraKind::kPerspective,
         "3D perspective");
  // Default pose must contain the normalized DEM (XZ diagonal), not a patch.
  {
    const float half_diag = (3.2f * 0.5f) * std::sqrt(2.f);
    const float visible =
        orbit->distance() * std::tan(content::kScene3dFovY * 0.5f);
    expect(visible + 1e-3f >= half_diag, "orbit frames full DEM");
  }
  // Product China orbit must not park inside the DEM sphere (half-cut), and
  // must keep FlyCube clip planes at (0.1, 100) — adaptive near washed DEM.
  {
    expect(std::fabs(orbit->distance() - content::kScene3dDefaultDistance) <
               1e-4f,
           "default distance frames DEM");
    expect(orbit->distance() + 1e-4f >= content::kScene3dDollyMinLocal,
           "default distance above local dolly floor");
    orbit->set_distance(1.45f);
    expect(orbit->distance() + 1e-4f >= content::kScene3dDollyMinLocal,
           "set_distance(1.45) clamps outside DEM sphere");
    const float dem_radius = content::kScene3dDefaultDistance * 0.55f;
    expect(orbit->distance() > dem_radius + 0.2f,
           "clamped eye outside DEM radius");
    const auto mats = orbit->camera_matrices(1.333f);
    constexpr float kNear = 0.1f;
    constexpr float kFar = 100.f;
    const float expect_p14 = (kNear * kFar) / (kNear - kFar);
    expect(std::fabs(mats.proj[14] - expect_p14) < 1e-4f,
           "orbit clip near=0.1 far=100");
    orbit->reset();
  }
  expect(orbit->camera_matrices_ortho(800.f, 600.f).kind ==
             render::rhi::CameraKind::kOrtho,
         "2D ortho");
  expect(content::extent_looks_like_china(orbit->world_extent()),
         "unbound extent is China");
  expect(cam->atmosphere_session().environment() == nullptr, "no atmosphere by default");

  const float yaw0 = orbit->yaw();
  const float dist0 = orbit->distance();
  orbit->apply_wheel_at(700, 80, 120, 800, 600);
  expect(orbit->distance() < dist0, "wheel dollies in");
  expect(std::fabs(orbit->yaw() - yaw0) > 1e-4f, "wheel-to-cursor yaws");

  orbit->reset();
  expect(std::fabs(orbit->yaw() - content::kScene3dDefaultYaw) < 1e-4f, "reset yaw");
  expect(std::fabs(orbit->distance() - 3.2f) < 1e-4f, "reset distance");

  const float dist1 = orbit->distance();
  orbit->apply_pinch(400, 300, 1.2, 800, 600);
  expect(orbit->distance() < dist1, "pinch-out dollies in");

  orbit->apply_pan(20, 0);
  expect(orbit->yaw() > content::kScene3dDefaultYaw, "pan yaws");

  orbit->reset();
  const float pitch0 = orbit->pitch();
  const float dist_before = orbit->distance();
  orbit->apply_pan(0, 80);
  expect(std::fabs(orbit->pitch() - pitch0) < 1e-4f, "pan does not pitch");
  expect(orbit->distance() != dist_before, "vertical pan dollies");

  // Edge-on pitch must clamp (thin green strip regression).
  {
    tool::Draft d;
    d.kind = tool::DraftKind::kRect;
    d.flags = 0x0002;  // MK_RBUTTON -orbit
    d.points.push_back({0, 0});
    d.points.push_back({0, -5000});
    orbit->apply_draft(d);
    expect(orbit->pitch() >= tool::kOrbitPitchMin - 0.01f, "orbit pitch floor");
  }

  content::Extent2 china = content::kChinaLonLatExtent;
  orbit->apply_world_extent(china);
  expect(content::extent_looks_like_china(orbit->world_extent()), "apply China");

  content::GisScene scene;
  cam->bind_scene(&scene);
  expect(content::extent_nonempty(orbit->world_extent()), "bound map has extent");

  // Seeded GisScene (China PLP) must still present DEM via World and WorldPass.
  // Heap-allocate Scene3dPresenter  -stack frame + WorldPass exceeds Debug RTC
  // comfort and has hung te on _RTC_StackFailure MessageBox.
  {
    content::GisScene seeded;
    seeded.seed_default();
    auto dem_orbit = std::make_unique<content::OrbitFrame>();
    auto dem_cam = std::make_unique<content::Scene3dPresenter>();
    dem_cam->bind_orbit(dem_orbit.get());
    dem_cam->bind_scene(&seeded);
    dem_orbit->apply_world_extent(seeded.world_extent());
    expect(content::extent_looks_like_china(dem_cam->world_extent()) ||
               content::extent_nonempty(dem_cam->world_extent()),
           "seeded map extent");
    std::unique_ptr<render::rhi::Device> dem_device(
        render::rhi::create_device(render::rhi::Backend::kNull));
    expect(dem_device != nullptr &&
               dem_device->initialize(render::rhi::DeviceDesc()),
           "null device for seeded DEM");
    expect(dem_cam->present_gpu(dem_device.get(), 64, 64),
           "present_gpu after seed_default (WorldPass + HudPass)");
    // Soft DEM removed — paint() is scenic MemFrame only / no-op.
    dem_cam->paint(nullptr, 64, 64, /*fill_background=*/true);
    dem_cam->abandon_mesh();
  }

  orbit->apply_draft(tool::Draft{});
  expect(!cam->hosts_shared_scene(), "no GisContents until bind_contents");

  // Atmosphere defaults off until enable_atmosphere_demo / setters.
  vista::atmosphere::Environment& env = cam->atmosphere_session().ensure();
  expect(!env.ocean_enabled() && !env.cloud_enabled(), "ensure keeps off");
  cam->atmosphere_session().enable_demo();
  expect(env.ocean_enabled() && env.cloud_enabled(), "demo enables ocean/cloud");
  expect(env.sky_enabled() && env.fog_enabled(), "demo enables sky/fog");
  expect(env.field_store().layer_count() > 0, "demo seeded fields");

  // Wind overlay: seeded WindU/V fed into vista HudPass on present_gpu.
  {
    auto wind_orbit = std::make_unique<content::OrbitFrame>();
    auto wind_cam = std::make_unique<content::Scene3dPresenter>();
    wind_cam->bind_orbit(wind_orbit.get());
    wind_cam->bind_scene(&scene);
    wind_cam->atmosphere_session().seed_procedural();
    wind_cam->atmosphere_session().set_wind_overlay_enabled(true);
    expect(wind_cam->atmosphere_session().wind_overlay_enabled(),
           "wind overlay on");
    wind_cam->atmosphere_session().set_time_sec(12.5);
    expect(std::abs(wind_cam->atmosphere_session().time_sec() - 12.5) < 1e-9,
           "time scrub");
    std::unique_ptr<render::rhi::Device> wind_device(
        render::rhi::create_device(render::rhi::Backend::kNull));
    expect(wind_device != nullptr &&
               wind_device->initialize(render::rhi::DeviceDesc()),
           "null device for wind HUD");
    expect(wind_cam->present_gpu(wind_device.get(), 64, 64),
           "present_gpu with wind HudPass");
    wind_cam->abandon_mesh();
  }

  // Ocean-only: seed without cloud pass.
  {
    auto ocean_only = std::make_unique<content::Scene3dPresenter>();
    ocean_only->bind_scene(&scene);
    ocean_only->atmosphere_session().seed_procedural();
    ocean_only->atmosphere_session().set_ocean_enabled(true);
    ocean_only->atmosphere_session().set_cloud_enabled(false);
    const vista::atmosphere::Environment* oenv =
        ocean_only->atmosphere_session().environment();
    expect(oenv && oenv->ocean_enabled() && !oenv->cloud_enabled(),
           "ocean-only flags");
    expect(oenv->field_store().layer_count() > 0, "ocean-only seeded");
  }

  // Null RHI present: ocean -land -clouds must not crash.
  {
    std::unique_ptr<render::rhi::Device> device(
        render::rhi::create_device(render::rhi::Backend::kNull));
    expect(device != nullptr, "null device");
    expect(device->initialize(render::rhi::DeviceDesc()), "null init");
    expect(cam->present_gpu(device.get(), 64, 64), "present with atmosphere");
    expect(std::strcmp(cam->render_engine_name(), "Null") == 0,
           "present_gpu sets Null engine label");

    // Disable and present again (land-only path still uses record_draws).
    cam->atmosphere_session().set_ocean_enabled(false);
    cam->atmosphere_session().set_cloud_enabled(false);
    cam->atmosphere_session().set_sky_enabled(false);
    cam->atmosphere_session().set_fog_enabled(false);
    expect(cam->present_gpu(device.get(), 64, 64), "present atmosphere off");

    // Drop GPU mesh/pass pointers before Device destruction (same as
    // MapViewport::detach ordering).
    cam->abandon_mesh();
  }

  // M3 city path: DEM + tiles stream/cache + atmosphere on/off hooks.
  {
    auto m3 = std::make_unique<content::Scene3dPresenter>();
    std::string m3_err;
    expect(m3->atmosphere_session().run_m3_self_test_hooks(&m3_err),
           "run_m3_self_test_hooks");
    expect(m3_err.empty(), "m3 hooks no error tag");
  }

  // Engine display names (HUD badge text comes from these on HudPass).
  {
    expect(std::strcmp(render::rhi::backend_display_name(
                           render::rhi::Backend::kDx12),
                       "FlyCube/DX12") == 0,
           "DX12 display name");
    expect(std::strcmp(render::rhi::backend_display_name(
                           render::rhi::Backend::kGl),
                       "Stereo/GL") == 0,
           "GL display name");
    expect(std::strcmp(render::rhi::backend_display_name(
                           render::rhi::Backend::kGdi),
                       "GDI") == 0,
           "GDI display name");
    expect(std::strcmp(render::rhi::backend_display_name(
                           render::rhi::Backend::kNull),
                       "Null") == 0,
           "Null display name");
  }

  // T2.1 prep mailbox: async stage overlaps Display drain without wait.
  {
    content::detail::Scene3dPrepMailbox mailbox;
    std::atomic<bool> worker_ran{false};
    (void)mailbox.stage_async(content::detail::Scene3dPrepAction::kColdRebuild,
                              [&](uint64_t /*gen*/) {
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(25));
                                worker_ran.store(true,
                                                 std::memory_order_release);
                              });
    for (int i = 0; i < 50; ++i) {
      if (worker_ran.load(std::memory_order_acquire)) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    content::detail::Scene3dPrepAction action =
        content::detail::Scene3dPrepAction::kWarmReuse;
    int64_t overlap_ms = 0;
    const bool drained =
        mailbox.drain(/*wait_if_empty=*/false, &action, &overlap_ms);
    expect(worker_ran.load(std::memory_order_acquire),
           "prep mailbox async worker ran");
    expect(drained, "prep mailbox async drain published");
    expect(action == content::detail::Scene3dPrepAction::kColdRebuild,
           "prep mailbox async action cold");
    expect(overlap_ms >= 20, "prep mailbox prep_overlap_ms measured");
    expect(mailbox.prep_build_count() == 1, "prep mailbox build count");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d scene3d_presenter_test fail(s)\n", g_fails);
    return 1;
  }
  return 0;
}
