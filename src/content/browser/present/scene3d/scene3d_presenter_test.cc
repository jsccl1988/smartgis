// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/software/scene3d_software_painter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "vista/world/terrain/dem/dem_frame.h"
#include "render/rhi/rhi.h"
#include "tool/nav/camera_nav.h"
#include "tool/draft/draft.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  // Default FlyCube; switch via set_scene3d_engine or SMT_SCENE3D_ENGINE.
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
    expect(content::force_content_mapview_3d(), "GDI forces content path");

    content::set_scene3d_engine(content::Scene3dEngine::kScenic);
    expect(content::prefer_scene3d_scenic(), "Scenic selected");
    expect(!content::prefer_scene3d_flycube(), "Scenic disables FlyCube");
    expect(!content::force_content_mapview_3d(),
           "Scenic does not force leftover HWND");
    {
      content::Scene3dPresenter cam;
      expect(cam.hosts_scenic_present(),
             "Scenic env/API hosts scenic.dll");
    }

    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
    expect(content::prefer_scene3d_flycube(), "restore FlyCube");
    expect(content::scene3d_engine() == content::Scene3dEngine::kFlyCube,
           "engine getter matches FlyCube");
  }

  // Harness env: SMT_SCENE3D_ENGINE selects leftover GL vs D3D under kStereoGl.
  {
    _putenv_s("SMT_SCENE3D_ENGINE", "stereo_gl");
    expect(content::apply_scene3d_engine_from_env(), "env stereo_gl applies");
    expect(content::prefer_scene3d_stereo_gl(), "env stereo_gl engine");
    expect(content::prefer_scene3d_stereo_opengl(), "env stereo_gl �?OpenGL");
    expect(!content::prefer_scene3d_stereo_d3d(), "env stereo_gl not D3D");

    _putenv_s("SMT_SCENE3D_ENGINE", "stereo_d3d");
    expect(content::apply_scene3d_engine_from_env(), "env stereo_d3d applies");
    expect(content::prefer_scene3d_stereo_gl(), "env stereo_d3d still stereo");
    expect(content::prefer_scene3d_stereo_d3d(), "env stereo_d3d �?D3D");
    expect(!content::prefer_scene3d_stereo_opengl(), "env stereo_d3d not GL");

    _putenv_s("SMT_SCENE3D_ENGINE", "scenic");
    expect(content::apply_scene3d_engine_from_env(), "env scenic applies");
    expect(content::prefer_scene3d_scenic(), "env scenic");

    _putenv_s("SMT_SCENE3D_ENGINE", "flycube");
    expect(content::apply_scene3d_engine_from_env(), "env flycube applies");
    expect(content::prefer_scene3d_flycube(), "env flycube");
    _putenv_s("SMT_SCENE3D_ENGINE", "");
    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
  }

  content::OrbitFrame orbit;
  content::Scene3dPresenter cam;
  cam.bind_orbit(&orbit);
  expect(!cam.hosts_shared_scene(), "fresh presenter has no MapContents");
  expect(std::fabs(content::kScene3dDefaultYaw - vista::kDemDefaultOrbitYaw) < 1e-6f,
         "host yaw aliases gis shared constant");
  expect(std::fabs(orbit.yaw() - vista::kDemDefaultOrbitYaw) < 1e-4f,
         "default yaw south-of-target");
  // make_orbit_camera: ez = dist * cos(pitch) * cos(yaw). South-of-target
  // requires ez < 0 so geographic +Z (north) sits toward the screen top.
  {
    const float ez = orbit.distance() * std::cos(orbit.pitch()) *
                     std::cos(orbit.yaw());
    expect(ez < 0.f, "default eye south of origin (north-up)");
  }
  // RH lookAt looking +Z �?camera right = -X; mesh X=-lon puts east on right.
  expect(vista::dem_lon_to_x(121.0) < vista::dem_lon_to_x(88.0),
         "east X < west X (screen-right looking north)");
  expect(orbit.camera_matrices(1.333f).kind ==
             render::rhi::CameraKind::kPerspective,
         "3D perspective");
  // Default pose must contain the normalized DEM (XZ diagonal), not a patch.
  {
    const float half_diag = (3.2f * 0.5f) * std::sqrt(2.f);
    const float visible =
        orbit.distance() * std::tan(content::kScene3dFovY * 0.5f);
    expect(visible + 1e-3f >= half_diag, "orbit frames full DEM");
  }
  expect(orbit.camera_matrices_ortho(800.f, 600.f).kind ==
             render::rhi::CameraKind::kOrtho,
         "2D ortho");
  expect(content::extent_looks_like_china(orbit.world_extent()),
         "unbound extent is China");
  expect(cam.atmosphere_session().environment() == nullptr, "no atmosphere by default");

  const float yaw0 = orbit.yaw();
  const float dist0 = orbit.distance();
  orbit.apply_wheel_at(700, 80, 120, 800, 600);
  expect(orbit.distance() < dist0, "wheel dollies in");
  expect(std::fabs(orbit.yaw() - yaw0) > 1e-4f, "wheel-to-cursor yaws");

  orbit.reset();
  expect(std::fabs(orbit.yaw() - content::kScene3dDefaultYaw) < 1e-4f, "reset yaw");
  expect(std::fabs(orbit.distance() - 3.2f) < 1e-4f, "reset distance");

  const float dist1 = orbit.distance();
  orbit.apply_pinch(400, 300, 1.2, 800, 600);
  expect(orbit.distance() < dist1, "pinch-out dollies in");

  orbit.apply_pan(20, 0);
  expect(orbit.yaw() > content::kScene3dDefaultYaw, "pan yaws");

  orbit.reset();
  const float pitch0 = orbit.pitch();
  const float dist_before = orbit.distance();
  orbit.apply_pan(0, 80);
  expect(std::fabs(orbit.pitch() - pitch0) < 1e-4f, "pan does not pitch");
  expect(orbit.distance() != dist_before, "vertical pan dollies");

  // Edge-on pitch must clamp (thin green strip regression).
  {
    tool::Draft d;
    d.kind = tool::DraftKind::kRect;
    d.flags = 0x0002;  // MK_RBUTTON �?orbit
    d.points.push_back({0, 0});
    d.points.push_back({0, -5000});
    orbit.apply_draft(d);
    expect(orbit.pitch() >= tool::kOrbitPitchMin - 0.01f, "orbit pitch floor");
  }

  content::Extent2 china = content::kChinaLonLatExtent;
  orbit.apply_world_extent(china);
  expect(content::extent_looks_like_china(orbit.world_extent()), "apply China");

  content::MapScene scene;
  cam.bind_map(&scene);
  expect(content::extent_nonempty(orbit.world_extent()), "bound map has extent");

  // Seeded MapScene (China PLP) must still present DEM via World �?GpuScene.
  {
    content::MapScene seeded;
    seeded.seed_default();
    content::OrbitFrame dem_orbit;
    content::Scene3dPresenter dem_cam;
    dem_cam.bind_orbit(&dem_orbit);
    dem_cam.bind_map(&seeded);
    dem_orbit.apply_world_extent(seeded.world_extent());
    expect(content::extent_looks_like_china(dem_cam.world_extent()) ||
               content::extent_nonempty(dem_cam.world_extent()),
           "seeded map extent");
    std::unique_ptr<render::rhi::Device> dem_device(
        render::rhi::create_device(render::rhi::Backend::kNull));
    expect(dem_device != nullptr &&
               dem_device->initialize(render::rhi::DeviceDesc()),
           "null device for seeded DEM");
    expect(dem_cam.present_gpu(dem_device.get(), 64, 64),
           "present_gpu after seed_default");
    // GDI DEM wireframe path (Views placeholder / late DIB).
    HDC screen = GetDC(nullptr);
    expect(screen != nullptr, "screen DC for GDI paint");
    if (screen) {
      HDC mem = CreateCompatibleDC(screen);
      BITMAPINFO bi = {};
      bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = 64;
      bi.bmiHeader.biHeight = -64;
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      bi.bmiHeader.biCompression = BI_RGB;
      void* bits = nullptr;
      HBITMAP dib =
          CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
      expect(mem && dib && bits, "GDI DIB for paint");
      if (mem && dib) {
        HGDIOBJ old = SelectObject(mem, dib);
        dem_cam.reset();
        dem_cam.paint(mem, 64, 64, /*fill_background=*/true);
        // Stride paint must cover the China AABB, not only the first mesh
        // rows (regression: thin green ribbon from first-N tris).
        if (bits) {
          const auto* px = static_cast<const std::uint32_t*>(bits);
          int min_x = 64, max_x = -1, min_y = 64, max_y = -1, hits = 0;
          for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
              const unsigned c = px[y * 64 + x];
              const unsigned r = c & 0xff;
              const unsigned g = (c >> 8) & 0xff;
              const unsigned b = (c >> 16) & 0xff;
              // Background clear is black; DEM fills are hypsometric (green /
              // yellow / pink). Count any non-black land-like pixel.
              if ((r + g + b) > 80 && !(r < 20 && g < 20 && b < 20)) {
                ++hits;
                min_x = (std::min)(min_x, x);
                max_x = (std::max)(max_x, x);
                min_y = (std::min)(min_y, y);
                max_y = (std::max)(max_y, y);
              }
            }
          }
          expect(hits > 80, "GDI DEM paints many land pixels");
          expect(max_x - min_x > 20 && max_y - min_y > 12,
                 "GDI DEM spans China AABB (not a ribbon)");
        }
        dem_cam.paint(mem, 64, 64, /*fill_background=*/false);
        dem_cam.paint_hud(mem, 64, 64);
        SelectObject(mem, old);
        DeleteObject(dib);
        DeleteDC(mem);
      }
      ReleaseDC(nullptr, screen);
    }
    dem_cam.abandon_mesh();
  }

  orbit.apply_draft(tool::Draft{});
  expect(!cam.hosts_shared_scene(), "no MapContents until bind_contents");

  // Atmosphere defaults off until enable_atmosphere_demo / setters.
  vista::atmosphere::Environment& env = cam.atmosphere_session().ensure();
  expect(!env.ocean_enabled() && !env.cloud_enabled(), "ensure keeps off");
  cam.atmosphere_session().enable_demo();
  expect(env.ocean_enabled() && env.cloud_enabled(), "demo enables ocean/cloud");
  expect(env.sky_enabled() && env.fog_enabled(), "demo enables sky/fog");
  expect(env.field_store().layer_count() > 0, "demo seeded fields");

  // Wind overlay: sample seeded WindU/V into GDI arrows (CPU path).
  {
    content::OrbitFrame wind_orbit;
    content::Scene3dPresenter wind_cam;
    wind_cam.bind_orbit(&wind_orbit);
    wind_cam.bind_map(&scene);
    wind_cam.atmosphere_session().seed_procedural();
    wind_cam.atmosphere_session().set_wind_overlay_enabled(true);
    expect(wind_cam.atmosphere_session().wind_overlay_enabled(), "wind overlay on");
    wind_cam.atmosphere_session().set_time_sec(12.5);
    expect(std::abs(wind_cam.atmosphere_session().time_sec() - 12.5) < 1e-9, "time scrub");
    HDC screen = GetDC(nullptr);
    if (screen) {
      HDC mem = CreateCompatibleDC(screen);
      BITMAPINFO bi = {};
      bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = 64;
      bi.bmiHeader.biHeight = -64;
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      void* bits = nullptr;
      HBITMAP dib =
          CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
      if (dib) {
        HGDIOBJ old = SelectObject(mem, dib);
        wind_cam.paint_hud(mem, 64, 64);
        SelectObject(mem, old);
        DeleteObject(dib);
      }
      DeleteDC(mem);
      ReleaseDC(nullptr, screen);
    }
    wind_cam.abandon_mesh();
  }

  // Ocean-only: seed without cloud pass.
  {
    content::Scene3dPresenter ocean_only;
    ocean_only.bind_map(&scene);
    ocean_only.atmosphere_session().seed_procedural();
    ocean_only.atmosphere_session().set_ocean_enabled(true);
    ocean_only.atmosphere_session().set_cloud_enabled(false);
    const vista::atmosphere::Environment* oenv = ocean_only.atmosphere_session().environment();
    expect(oenv && oenv->ocean_enabled() && !oenv->cloud_enabled(),
           "ocean-only flags");
    expect(oenv->field_store().layer_count() > 0, "ocean-only seeded");
  }

  // Null RHI present: ocean �?land �?clouds must not crash.
  {
    std::unique_ptr<render::rhi::Device> device(
        render::rhi::create_device(render::rhi::Backend::kNull));
    expect(device != nullptr, "null device");
    expect(device->initialize(render::rhi::DeviceDesc()), "null init");
    expect(cam.present_gpu(device.get(), 64, 64), "present with atmosphere");
    expect(std::strcmp(cam.render_engine_name(), "Null") == 0,
           "present_gpu sets Null engine label");

    // Disable and present again (land-only path still uses record_draws).
    cam.atmosphere_session().set_ocean_enabled(false);
    cam.atmosphere_session().set_cloud_enabled(false);
    cam.atmosphere_session().set_sky_enabled(false);
    cam.atmosphere_session().set_fog_enabled(false);
    expect(cam.present_gpu(device.get(), 64, 64), "present atmosphere off");

    // Drop GPU mesh/pass pointers before Device destruction (same as
    // MapViewport::detach ordering).
    cam.abandon_mesh();
  }

  // M3 city path: DEM + tiles stream/cache + atmosphere on/off hooks.
  {
    content::Scene3dPresenter m3;
    std::string m3_err;
    expect(m3.atmosphere_session().run_m3_self_test_hooks(&m3_err), "run_m3_self_test_hooks");
    expect(m3_err.empty(), "m3 hooks no error tag");
  }

  // Engine logo badge: bottom-right dark box with light text on a mem DC.
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

    HDC screen = GetDC(nullptr);
    expect(screen != nullptr, "screen DC for logo");
    if (screen) {
      HDC mem = CreateCompatibleDC(screen);
      BITMAPINFO bi = {};
      bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = 160;
      bi.bmiHeader.biHeight = -120;
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      void* bits = nullptr;
      HBITMAP dib =
          CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
      expect(dib != nullptr && bits != nullptr, "logo DIB");
      if (dib && bits) {
        HGDIOBJ old = SelectObject(mem, dib);
        auto* px = static_cast<uint32_t*>(bits);
        for (int i = 0; i < 160 * 120; ++i) {
          px[i] = 0xFF808080u;  // mid gray so the dark badge is distinct
        }
        content::Scene3dSoftwarePainter::paint_engine_logo(mem, 160, 120,
                                                    "ContentMapView");
        // Sample bottom-right margin (badge sits ~14px inset).
        const int sx = 160 - 20;
        const int sy = 120 - 20;
        const uint32_t sample = px[sy * 160 + sx];
        const uint8_t b = static_cast<uint8_t>(sample & 0xFFu);
        const uint8_t g = static_cast<uint8_t>((sample >> 8) & 0xFFu);
        const uint8_t r = static_cast<uint8_t>((sample >> 16) & 0xFFu);
        expect(r < 40 && g < 50 && b < 60, "logo badge is dark bottom-right");
        SelectObject(mem, old);
        DeleteObject(dib);
      }
      DeleteDC(mem);
      ReleaseDC(nullptr, screen);
    }
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d scene3d_presenter_test fail(s)\n", g_fails);
    return 1;
  }
  return 0;
}
