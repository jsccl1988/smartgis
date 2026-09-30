// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/atmosphere_showcase.h"

#include <windows.h>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/bmp.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/common/pump.h"
#include "app/views/shell/harness/showcase/atmosphere/host.h"
#include "app/views/shell/harness/showcase/atmosphere/linger.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/map_host_extent.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/map_viewport.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using app::AtmosphereShowcaseMode;
using app::atmosphere_showcase_name;
using app::detail::AtmosphereShowcaseLinger;
using app::detail::atmosphere_showcase_linger;
using app::detail::create_atmosphere_showcase_hwnd;
using app::detail::bmp_file_has_visible_signal;
using app::detail::capture_hwnd_bmp;
using app::detail::detach_maps;
using app::detail::pump_messages;
using app::detail::write_mark;
using app::detail::kAtmosphereShowcaseMarkLeaf;

constexpr uint32_t kAtmosphereShowcaseW = 640;
constexpr uint32_t kAtmosphereShowcaseH = 480;

void showcase_mark(const char* step) {
  write_mark(kAtmosphereShowcaseMarkLeaf, step, /*truncate=*/false);
}

int run_atmosphere_showcase_impl(app::Browser& browser,
                                 AtmosphereShowcaseMode mode) {
  const char* name = atmosphere_showcase_name(mode);
  std::fprintf(stderr, "atmosphere-showcase mode=%s\n", name);
  showcase_mark(name);

  browser.select_map_tab(2);
  showcase_mark("tab3d");
  pump_messages(600);
  showcase_mark("pumped");
  ui::views::MapViewport* scene = browser.map_scene_viewport();
  if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
    std::fprintf(stderr, "atmosphere-showcase: 3D viewport HWND missing\n");
    detach_maps(browser);
    return 50;
  }
  showcase_mark("scene-hwnd-ok");

  // Default: Null RHI (deterministic exit). Set SMT_ATMOSPHERE_SHOWCASE_GPU=1
  // for FlyCube/DX12 on a dedicated 640x480 present window (not the tiny tab
  // child). GPU lingers until the present HWND is closed; CI may set
  // SMT_ATMOSPHERE_SHOWCASE_TIMED_MS, or LINGER_MS=0 to skip.
  const bool want_gpu = []() {
    if (const char* env = std::getenv("SMT_ATMOSPHERE_SHOWCASE_GPU")) {
      return env[0] == '1' && env[1] == '\0';
    }
    return false;
  }();
  const AtmosphereShowcaseLinger linger = atmosphere_showcase_linger(want_gpu);

  // Drop any ContentMapView / prior FlyCube on the tab before we own a device.
  if (scene->attach_mode() ==
          ui::views::MapViewport::AttachMode::kContentMapView ||
      scene->attach_mode() == ui::views::MapViewport::AttachMode::kFlyCube) {
    scene->detach();
    showcase_mark("detached");
    pump_messages(100);
  }

  HWND present_hwnd = nullptr;
  HWND owned_present_hwnd = nullptr;
  if (want_gpu) {
    owned_present_hwnd =
        create_atmosphere_showcase_hwnd(kAtmosphereShowcaseW,
                                        kAtmosphereShowcaseH);
    if (!owned_present_hwnd) {
      std::fprintf(stderr, "atmosphere-showcase: present HWND create failed\n");
      detach_maps(browser);
      return 50;
    }
    present_hwnd = owned_present_hwnd;
    showcase_mark("present-hwnd-ok");
  } else {
    present_hwnd = scene->native_view();
    if (!present_hwnd) {
      scene->realize_native();
      present_hwnd = scene->native_view();
    }
    if (!present_hwnd) {
      std::fprintf(stderr, "atmosphere-showcase: HWND gone after detach\n");
      detach_maps(browser);
      return 50;
    }
  }
  showcase_mark("hwnd-ready");

  render::rhi::Device* device = render::rhi::create_device(
      want_gpu ? render::rhi::preferred_gpu_backend()
               : render::rhi::Backend::kNull);
  if (!device) {
    std::fprintf(stderr, "atmosphere-showcase: create_device failed\n");
    showcase_mark("device-missing");
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 51;
  }
  showcase_mark("device-created");
  const bool owns_device = true;

  render::rhi::DeviceDesc desc;
  desc.native_window = want_gpu ? present_hwnd : nullptr;
  desc.width = kAtmosphereShowcaseW;
  desc.height = kAtmosphereShowcaseH;
  std::fprintf(stderr,
               "atmosphere-showcase: gpu=%d linger=%s present=%p %ux%u\n",
               want_gpu ? 1 : 0,
               linger.until_close ? "until-close"
                                  : (linger.ms > 0 ? "timed" : "none"),
               static_cast<void*>(present_hwnd), desc.width, desc.height);
  if (!linger.until_close && linger.ms > 0) {
    std::fprintf(stderr, "atmosphere-showcase: linger_ms=%lu\n",
                 static_cast<unsigned long>(linger.ms));
  }
  if (!device->initialize(desc)) {
    std::fprintf(stderr, "atmosphere-showcase: device initialize failed\n");
    device->shutdown();
    showcase_mark("device-missing");
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 51;
  }
  if (want_gpu) {
    if (render::rhi::CommandList* warm = device->create_command_list()) {
      render::rhi::RenderPassDesc pass;
      pass.clear_r = 0.05f;
      pass.clear_g = 0.12f;
      pass.clear_b = 0.18f;
      pass.clear_a = 1.f;
      pass.width = desc.width;
      pass.height = desc.height;
      warm->begin_render_pass(pass);
      warm->set_viewport(0, 0, static_cast<float>(desc.width),
                         static_cast<float>(desc.height), 0, 1);
      warm->end_render_pass();
      warm->close();
      device->execute(warm);
      device->destroy_command_list(warm);
      device->present();
    }
  }
  showcase_mark(want_gpu ? "device-init-gpu" : "device-init-null");
  showcase_mark(want_gpu ? "flycube-ok" : "null-ok");

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 50;
  }

  // Keep the China DEM centered for BMP capture (no orbit nudge that looks
  // at empty ocean and fails the landish visual gate).
  showcase_mark("orbit-reset");
  orbit->reset();
  orbit->apply_world_extent(content::kChinaLonLatExtent);
  // Dolly in so the DEM fills enough of the 640x480 frame for landish gates
  // (default 3.2 leaves sky/ocean dominating mid-band).
  if (mode == AtmosphereShowcaseMode::kFull ||
      mode == AtmosphereShowcaseMode::kLand) {
    orbit->set_distance(2.55f);
  }
  showcase_mark("extent-ok");

  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      // DEM / land present only. Explicitly clear any session flags left on
      // from Browser init / prior seed (otherwise land gate exits 53).
      cam->atmosphere_session().set_ocean_enabled(false);
      cam->atmosphere_session().set_cloud_enabled(false);
      cam->atmosphere_session().set_sky_enabled(false);
      cam->atmosphere_session().set_fog_enabled(false);
      break;
    case AtmosphereShowcaseMode::kOcean:
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(false);
      break;
    case AtmosphereShowcaseMode::kFull:
      // Mid-tier full: ocean + soft cloud + sky + fog. Cover is capped in
      // prepare_clouds; ocean deep-navy albedo avoids cyan far-field wash.
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(true);
      break;
    case AtmosphereShowcaseMode::kCoast: {
      // East China Sea coastal window â€?different extent from full China.
      const content::Extent2 coast{118.0, 28.0, 128.0, 36.0};
      orbit->apply_world_extent(coast);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(true);
      break;
    }
    case AtmosphereShowcaseMode::kNone:
    default:
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      detach_maps(browser);
      return 53;
  }
  showcase_mark("demo-ok");

  const gis::atmosphere::Environment* env =
      cam->atmosphere_session().environment();
  // Full / coast: ocean + soft cloud + sky + fog.
  const bool want_ocean = mode == AtmosphereShowcaseMode::kOcean ||
                          mode == AtmosphereShowcaseMode::kCoast ||
                          mode == AtmosphereShowcaseMode::kFull;
  const bool want_cloud = mode == AtmosphereShowcaseMode::kFull ||
                          mode == AtmosphereShowcaseMode::kCoast;
  const bool want_sky = mode == AtmosphereShowcaseMode::kFull ||
                        mode == AtmosphereShowcaseMode::kCoast;
  const bool want_fog = mode == AtmosphereShowcaseMode::kCoast ||
                        mode == AtmosphereShowcaseMode::kFull;
  if (mode == AtmosphereShowcaseMode::kLand) {
    if (env && (env->ocean_enabled() || env->cloud_enabled() ||
                env->sky_enabled() || env->fog_enabled())) {
      std::fprintf(stderr,
                   "atmosphere-showcase: land mode still has passes on\n");
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      detach_maps(browser);
      return 53;
    }
  } else {
    if (!env) {
      std::fprintf(stderr, "atmosphere-showcase: Environment missing\n");
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      detach_maps(browser);
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
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      detach_maps(browser);
      return 53;
    }
    if (env->field_store().layer_count() == 0) {
      std::fprintf(stderr, "atmosphere-showcase: FieldStore empty\n");
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      detach_maps(browser);
      return 53;
    }
  }
  showcase_mark("config-ok");
  std::fprintf(stderr, "atmosphere-showcase: ocean=%d cloud=%d layers=%zu\n",
               env && env->ocean_enabled() ? 1 : 0,
               env && env->cloud_enabled() ? 1 : 0,
               env ? env->field_store().layer_count() : 0u);

  const uint32_t kW = kAtmosphereShowcaseW;
  const uint32_t kH = kAtmosphereShowcaseH;
  int presents = 0;
  auto present_one = [&](const char* mark) -> bool {
    showcase_mark(mark);
    if (!cam->present_gpu(device, kW, kH)) {
      return false;
    }
    ++presents;
    pump_messages(50);
    return true;
  };
  for (int i = 0; i < 3; ++i) {
    char frame_mark[32];
    std::snprintf(frame_mark, sizeof(frame_mark), "present-%d", i);
    if (!present_one(frame_mark)) {
      std::fprintf(stderr, "atmosphere-showcase: present_gpu failed frame %d\n",
                   i);
      cam->abandon_mesh();
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      detach_maps(browser);
      return 52;
    }
  }
  showcase_mark("present-ok");
  std::fprintf(stderr, "atmosphere-showcase: presented %d frames %ux%u\n",
               presents, kW, kH);

  // Interactive linger: keep presenting so ocean/cloud stay visible.
  // Capture while the present HWND is still alive (before until-close ends).
  auto capture_showcase_bmp = [&]() -> bool {
    wchar_t bmp_path[MAX_PATH] = {};
    wchar_t file[64] = {};
    swprintf_s(file, L"atmosphere-showcase-%S.bmp", name);
    if (!app::detail::exe_sidecar_path(bmp_path, MAX_PATH, file)) {
      return !want_gpu;
    }
    HWND capture_hwnd =
        owned_present_hwnd ? owned_present_hwnd : present_hwnd;
    (void)cam->present_gpu(device, kW, kH);
    pump_messages(80);
    if (!capture_hwnd_bmp(capture_hwnd, bmp_path)) {
      showcase_mark("bmp-skip");
      std::fprintf(stderr, "atmosphere-showcase: BMP capture skipped\n");
      return !want_gpu;
    }
    int bw = 0;
    int bh = 0;
    app::detail::BmpFileCheckOpts check;
    check.require_color_diversity = true;
    const bool signal =
        bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
    std::fwprintf(stderr,
                  L"atmosphere-showcase: wrote %ls (%dx%d signal=%d)\n",
                  bmp_path, bw, bh, signal ? 1 : 0);
    if (signal) {
      showcase_mark("bmp-ok");
      return true;
    }
    showcase_mark("bmp-black");
    std::fprintf(stderr, "atmosphere-showcase: BMP lacks visible signal\n");
    return false;
  };

  bool bmp_signal_ok = !want_gpu;
  if (linger.until_close || linger.ms > 0) {
    showcase_mark("linger-start");
    if (linger.until_close) {
      std::fprintf(stderr,
                   "atmosphere-showcase: linger until window closed "
                   "(close the showcase window when done)\n");
    } else {
      std::fprintf(stderr, "atmosphere-showcase: linger %lu ms\n",
                   static_cast<unsigned long>(linger.ms));
    }
    const DWORD linger_end =
        linger.until_close ? 0u : (GetTickCount() + linger.ms);
    int linger_frames = 0;
    bool captured = false;
    for (;;) {
      if (owned_present_hwnd && !IsWindow(owned_present_hwnd)) {
        break;
      }
      if (!linger.until_close && GetTickCount() >= linger_end) {
        break;
      }
      if (!cam->present_gpu(device, kW, kH)) {
        std::fprintf(stderr, "atmosphere-showcase: linger present failed\n");
        break;
      }
      ++linger_frames;
      if (!captured && linger_frames >= 8) {
        bmp_signal_ok = capture_showcase_bmp();
        captured = true;
      }
      pump_messages(33);
    }
    if (!captured) {
      bmp_signal_ok = capture_showcase_bmp();
    }
    showcase_mark("linger-ok");
    std::fprintf(stderr, "atmosphere-showcase: linger frames=%d\n",
                 linger_frames);
  } else if (want_gpu) {
    bmp_signal_ok = capture_showcase_bmp();
  }

  cam->abandon_mesh();
  if (owns_device && device) {
    device->shutdown();
    // Intentionally leak Device* â€?FlyCube teardown has corrupted heaps
    // when operator delete runs after a live DX12 session (see MapViewport).
  }
  if (owned_present_hwnd) {
    DestroyWindow(owned_present_hwnd);
    owned_present_hwnd = nullptr;
  }
  detach_maps(browser);
  if (want_gpu && !bmp_signal_ok) {
    showcase_mark("bmp-fail");
    std::fprintf(stderr, "atmosphere-showcase: FAIL mode=%s (exit 54)\n", name);
    return 54;
  }
  showcase_mark("pass");
  std::fprintf(stderr, "atmosphere-showcase: PASS mode=%s\n", name);
  return 0;
}

}  // namespace

namespace app {

int run_atmosphere_showcase(Browser& browser, AtmosphereShowcaseMode mode) {
  return run_atmosphere_showcase_impl(browser, mode);
}

}  // namespace app
