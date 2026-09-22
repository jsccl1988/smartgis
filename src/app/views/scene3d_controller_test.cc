// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/scene3d_controller.h"

#include "app/views/map_host_extent.h"
#include "app/views/map_scene.h"
#include "app/views/scene3d_rhi_session.h"
#include "gis/world/dem_frame.h"
#include "render/rhi/rhi.h"
#include "tool/camera_nav.h"
#include "tool/gestures.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
  // Default: FlyCube RHI; opt out via FORCE_CONTENT=1 or PREFER_FLYCUBE=0.
  {
    _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_3D", "");
    _putenv_s("SMT_PREFER_FLYCUBE_3D", "");
    expect(app::prefer_scene3d_flycube(), "default prefer FlyCube RHI");
    expect(!app::force_content_mapview_3d(), "default not force content");
    _putenv_s("SMT_PREFER_FLYCUBE_3D", "1");
    expect(app::prefer_scene3d_flycube(), "PREFER_FLYCUBE=1 keeps FlyCube");
    _putenv_s("SMT_PREFER_FLYCUBE_3D", "");
    _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_3D", "1");
    expect(app::force_content_mapview_3d(), "FORCE_CONTENT=1");
    expect(!app::prefer_scene3d_flycube(), "FORCE_CONTENT disables FlyCube");
    _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_3D", "");
    _putenv_s("SMT_PREFER_FLYCUBE_3D", "0");
    expect(app::force_content_mapview_3d(), "PREFER_FLYCUBE=0 forces content");
    expect(!app::prefer_scene3d_flycube(), "PREFER_FLYCUBE=0 disables FlyCube");
    _putenv_s("SMT_PREFER_FLYCUBE_3D", "");
  }

  app::Scene3dController cam;
  expect(std::fabs(app::kScene3dDefaultYaw - gis::kDemDefaultOrbitYaw) < 1e-6f,
         "host yaw aliases gis shared constant");
  expect(std::fabs(cam.yaw() - gis::kDemDefaultOrbitYaw) < 1e-4f,
         "default yaw south-of-target");
  // make_orbit_camera: ez = dist * cos(pitch) * cos(yaw). South-of-target
  // requires ez < 0 so geographic +Z (north) sits toward the screen top.
  {
    const float ez = cam.distance() * std::cos(cam.pitch()) *
                     std::cos(cam.yaw());
    expect(ez < 0.f, "default eye south of origin (north-up)");
  }
  // RH lookAt looking +Z → camera right = -X; mesh X=-lon puts east on right.
  expect(gis::dem_lon_to_x(121.0) < gis::dem_lon_to_x(88.0),
         "east X < west X (screen-right looking north)");
  expect(cam.camera_matrices(1.333f).kind ==
             render::rhi::CameraKind::kPerspective,
         "3D perspective");
  expect(cam.camera_matrices_ortho(800.f, 600.f).kind ==
             render::rhi::CameraKind::kOrtho,
         "2D ortho");
  expect(app::extent_looks_like_china(cam.world_extent()),
         "unbound extent is China");
  expect(cam.atmosphere() == nullptr, "no atmosphere by default");

  const float yaw0 = cam.yaw();
  const float dist0 = cam.distance();
  cam.apply_wheel_at(700, 80, 120, 800, 600);
  expect(cam.distance() < dist0, "wheel dollies in");
  expect(std::fabs(cam.yaw() - yaw0) > 1e-4f, "wheel-to-cursor yaws");

  cam.reset();
  expect(std::fabs(cam.yaw() - app::kScene3dDefaultYaw) < 1e-4f, "reset yaw");
  expect(std::fabs(cam.distance() - 3.2f) < 1e-4f, "reset distance");

  const float dist1 = cam.distance();
  cam.apply_pinch(400, 300, 1.2, 800, 600);
  expect(cam.distance() < dist1, "pinch-out dollies in");

  cam.apply_pan(20, 0);
  expect(cam.yaw() > app::kScene3dDefaultYaw, "pan yaws");

  cam.reset();
  const float pitch0 = cam.pitch();
  const float dist_before = cam.distance();
  cam.apply_pan(0, 80);
  expect(std::fabs(cam.pitch() - pitch0) < 1e-4f, "pan does not pitch");
  expect(cam.distance() != dist_before, "vertical pan dollies");

  // Edge-on pitch must clamp (thin green strip regression).
  {
    tool::Draft d;
    d.kind = tool::DraftKind::kRect;
    d.flags = 0x0002;  // MK_RBUTTON → orbit
    d.points.push_back({0, 0});
    d.points.push_back({0, -5000});
    cam.apply_draft(d);
    expect(cam.pitch() >= tool::kOrbitPitchMin - 0.01f, "orbit pitch floor");
  }

  content::Extent2 china = app::kChinaLonLatExtent;
  cam.apply_world_extent(china);
  expect(app::extent_looks_like_china(cam.world_extent()), "apply China");

  app::MapScene scene;
  cam.bind_map(&scene);
  expect(app::extent_nonempty(cam.world_extent()), "bound map has extent");

  // Seeded MapScene (China PLP) must still present DEM via World → GpuScene.
  {
    app::MapScene seeded;
    seeded.seed_default();
    app::Scene3dController dem_cam;
    dem_cam.bind_map(&seeded);
    dem_cam.apply_world_extent(seeded.world_extent());
    expect(app::extent_looks_like_china(dem_cam.world_extent()) ||
               app::extent_nonempty(dem_cam.world_extent()),
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

  cam.apply_draft(tool::Draft{});
  expect(!cam.hosts_shared_scene(), "no MapContents until bind_contents");

  // Atmosphere defaults off until enable_atmosphere_demo / setters.
  gis::atmosphere::Environment& env = cam.ensure_atmosphere();
  expect(!env.ocean_enabled() && !env.cloud_enabled(), "ensure keeps off");
  cam.enable_atmosphere_demo();
  expect(env.ocean_enabled() && env.cloud_enabled(), "demo enables both");
  expect(env.field_store().layer_count() > 0, "demo seeded fields");

  // Wind overlay: sample seeded WindU/V into GDI arrows (CPU path).
  {
    app::Scene3dController wind_cam;
    wind_cam.bind_map(&scene);
    wind_cam.seed_atmosphere_procedural();
    wind_cam.set_wind_overlay_enabled(true);
    expect(wind_cam.wind_overlay_enabled(), "wind overlay on");
    wind_cam.set_time_sec(12.5);
    expect(std::abs(wind_cam.time_sec() - 12.5) < 1e-9, "time scrub");
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
    app::Scene3dController ocean_only;
    ocean_only.bind_map(&scene);
    ocean_only.seed_atmosphere_procedural();
    ocean_only.set_ocean_enabled(true);
    ocean_only.set_cloud_enabled(false);
    const gis::atmosphere::Environment* oenv = ocean_only.atmosphere();
    expect(oenv && oenv->ocean_enabled() && !oenv->cloud_enabled(),
           "ocean-only flags");
    expect(oenv->field_store().layer_count() > 0, "ocean-only seeded");
  }

  // Null RHI present: ocean → land → clouds must not crash.
  {
    std::unique_ptr<render::rhi::Device> device(
        render::rhi::create_device(render::rhi::Backend::kNull));
    expect(device != nullptr, "null device");
    expect(device->initialize(render::rhi::DeviceDesc()), "null init");
    expect(cam.present_gpu(device.get(), 64, 64), "present with atmosphere");

    // Disable and present again (land-only path still uses record_draws).
    cam.set_ocean_enabled(false);
    cam.set_cloud_enabled(false);
    expect(cam.present_gpu(device.get(), 64, 64), "present atmosphere off");

    // Drop GPU mesh/pass pointers before Device destruction (same as
    // MapViewport::detach ordering).
    cam.abandon_mesh();
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d scene3d_controller_test fail(s)\n", g_fails);
    return 1;
  }
  return 0;
}
