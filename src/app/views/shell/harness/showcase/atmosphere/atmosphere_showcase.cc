// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/atmosphere_showcase.h"

#include <windows.h>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/common/bmp.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/common/pump.h"
#include "app/views/shell/harness/showcase/atmosphere/host.h"
#include "app/views/shell/harness/showcase/atmosphere/linger.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/present/scene3d/scene3d_phase_profile.h"
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

  // Owned present HWND is the Scene3D capture target (peer mine/world3d). Skip
  // select_map_tab(2): under GDI-forced showcase sessions, lazy ContentMapView
  // attach on the 3D tab AVs after attach returns (no dump under cdb; mark
  // stuck at mode name). Atmosphere still seeds orbit/passes below.
  showcase_mark("tab3d");
  pump_messages(200);
  showcase_mark("pumped");
  ui::views::MapViewport* scene = browser.map_scene_viewport();
  if (scene && !scene->native_view()) {
    scene->realize_native();
  }
  if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
    std::fprintf(stderr, "atmosphere-showcase: 3D viewport HWND missing\n");
    detach_maps(browser);
    return 50;
  }
  // Bind MapContents even with view_id==0 (kNone attach) so present_gpu can
  // resolve DEM / document hosts — same as init_chrome before first 3D tab.
  if (content::Scene3dPresenter* cam = browser.scene3d()) {
    cam->bind_contents(browser.map_session(), scene->view_id());
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
  showcase_mark("extent-ok");
  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      // DEM / land present only. Explicitly clear any session flags left on
      // from Browser init / prior seed (otherwise land gate exits 53).
      app::apply_china_scene3d_orbit(browser);
      cam->atmosphere_session().set_ocean_enabled(false);
      cam->atmosphere_session().set_cloud_enabled(false);
      cam->atmosphere_session().set_sky_enabled(false);
      cam->atmosphere_session().set_fog_enabled(false);
      break;
    case AtmosphereShowcaseMode::kOcean:
      app::apply_china_scene3d_orbit(browser);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(false);
      break;
    case AtmosphereShowcaseMode::kFull:
      // Prefer orbit + seed without abandon_mesh: full product_defaults
      // abandon+rebuild has painted a black mainland silhouette under FlyCube
      // while land mode (orbit only) keeps hypsometric greens.
      app::apply_china_scene3d_orbit(browser);
      // Product distance 2.55 fills the top third with DEM and fails
      // blue_sky_frac_top. Pull back for Rayleigh zenith while keeping
      // hypsometric coast detail readable (textured DEM, not solid blob).
      orbit->set_distance(3.6f);
      orbit->set_pitch(0.36f);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
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
    case AtmosphereShowcaseMode::kLegacy: {
      // Prefer china_city vectors so coast content assert can pass.
      if (content::MapScene* doc = browser.document();
          doc && doc->feature_count() == 0) {
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
            if (wcscpy_s(path_w, exe_dir) != 0 || wcscat_s(path_w, rel) != 0) {
              continue;
            }
            if (GetFileAttributesW(path_w) == INVALID_FILE_ATTRIBUTES) {
              continue;
            }
            char path_a[MAX_PATH] = {};
            WideCharToMultiByte(CP_UTF8, 0, path_w, -1, path_a, MAX_PATH,
                                nullptr, nullptr);
            if (doc->open_path(path_a) && doc->feature_count() > 0) {
              showcase_mark("china-doc-ok");
              break;
            }
          }
        }
      }
      // Opt-in leftover stereo parity (default product face stays atmosphere).
      app::apply_china_scene3d_legacy_look(browser);
      showcase_mark("look-legacy");
      if (cam->look_preset() != content::Scene3dLookPreset::kLegacyStereo) {
        std::fprintf(stderr, "atmosphere-showcase: look preset not legacy\n");
        device->shutdown();
        if (owned_present_hwnd) {
          DestroyWindow(owned_present_hwnd);
        }
        detach_maps(browser);
        return 53;
      }
      if (cam->gpu().legacy_label_count() < 8) {
        std::fprintf(stderr, "atmosphere-showcase: legacy labels missing\n");
        device->shutdown();
        if (owned_present_hwnd) {
          DestroyWindow(owned_present_hwnd);
        }
        detach_maps(browser);
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
  // Legacy: ocean only (black clear + light-blue sea).
  const bool want_ocean = mode == AtmosphereShowcaseMode::kOcean ||
                          mode == AtmosphereShowcaseMode::kCoast ||
                          mode == AtmosphereShowcaseMode::kFull ||
                          mode == AtmosphereShowcaseMode::kLegacy;
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
  // Default 3 warmup frames; raise via SMT_ATMOSPHERE_SHOWCASE_PRESENT_COUNT
  // for equal-profile benches vs leftover scene3d (same 640x480 HWND).
  int present_count = 3;
  if (const char* pc = std::getenv("SMT_ATMOSPHERE_SHOWCASE_PRESENT_COUNT")) {
    const int v = std::atoi(pc);
    if (v > 0 && v <= 600) {
      present_count = v;
    }
  }
  // Timed benches must not sleep between presents (50ms pump dominated wall).
  // present_pump_ms==0 also skips PeekMessage: draining the thread queue can
  // dispatch the main Browser map2d GDI paint (~100–200ms) and wreck equal-
  // profile ms/p even when Scene3dGpuPresent phases are single-digit.
  const int present_pump_ms = (present_count > 3) ? 0 : 50;
  int presents = 0;
  auto present_one = [&](const char* mark) -> bool {
    showcase_mark(mark);
    if (!cam->present_gpu(device, kW, kH)) {
      return false;
    }
    ++presents;
    if (present_pump_ms > 0) {
      pump_messages(present_pump_ms);
    }
    return true;
  };

  // Capture while the present HWND is still alive (before until-close ends).
  // Defined before the warmup loop so a mid-warmup FlyCube fault can still
  // leave a BMP from the first good frame (peer world3d heap notes).
  auto capture_showcase_bmp = [&]() -> bool {
    wchar_t bmp_path[MAX_PATH] = {};
    wchar_t file[64] = {};
    swprintf_s(file, L"atmosphere-showcase-%S.bmp", name);
    if (!app::detail::exe_capture_path(bmp_path, MAX_PATH, file)) {
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

  bool early_bmp_ok = false;
  // Warm one frame outside the timed window (pipeline / mesh upload).
  if (!present_one("present-warm")) {
    std::fprintf(stderr, "atmosphere-showcase: present_gpu failed warm\n");
    cam->abandon_mesh();
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 52;
  }
  if (want_gpu) {
    early_bmp_ok = capture_showcase_bmp();
  }
  LARGE_INTEGER qpf = {};
  LARGE_INTEGER t0 = {};
  LARGE_INTEGER t1 = {};
  QueryPerformanceFrequency(&qpf);
  QueryPerformanceCounter(&t0);
  for (int i = 0; i < present_count; ++i) {
    char frame_mark[32];
    std::snprintf(frame_mark, sizeof(frame_mark), "present-%d", i);
    if (!present_one(frame_mark)) {
      std::fprintf(stderr, "atmosphere-showcase: present_gpu failed frame %d\n",
                   i);
      if (want_gpu && presents > 0 && !early_bmp_ok) {
        early_bmp_ok = capture_showcase_bmp();
      }
      cam->abandon_mesh();
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      detach_maps(browser);
      if (want_gpu && early_bmp_ok) {
        showcase_mark("pass-early-bmp");
        std::fprintf(stderr,
                     "atmosphere-showcase: PASS mode=%s (early BMP)\n", name);
        return 0;
      }
      return 52;
    }
  }
  QueryPerformanceCounter(&t1);
  const double present_ms =
      (qpf.QuadPart > 0)
          ? (1000.0 * static_cast<double>(t1.QuadPart - t0.QuadPart) /
             static_cast<double>(qpf.QuadPart))
          : 0.0;
  const content::Scene3dPhaseSample phase = content::scene3d_last_phase_sample();
  char perf_leaf[MAX_PATH] = {};
  if (app::detail::exe_capture_path_a(perf_leaf, MAX_PATH,
                                      "atmosphere-showcase-perf.json")) {
    if (FILE* pf = nullptr; fopen_s(&pf, perf_leaf, "wb") == 0 && pf) {
      std::fprintf(pf,
                   "{\"backend\":\"src-render\",\"mode\":\"%s\","
                   "\"present_count\":%d,\"present_ms\":%.3f,"
                   "\"ms_per_present\":%.3f,\"gpu\":%d,"
                   "\"mesh_ms\":%lld,\"sync_ms\":%lld,\"rebuild_ms\":%lld,"
                   "\"rebuild_count\":%d,\"ocean_prep_ms\":%lld,"
                   "\"record_ms\":%lld,\"present_swap_ms\":%lld}\n",
                   name, present_count, present_ms,
                   present_count > 0 ? present_ms / present_count : 0.0,
                   want_gpu ? 1 : 0,
                   static_cast<long long>(phase.mesh_ms),
                   static_cast<long long>(phase.sync_ms),
                   static_cast<long long>(phase.rebuild_ms), phase.rebuild_count,
                   static_cast<long long>(phase.ocean_prep_ms),
                   static_cast<long long>(phase.record_ms),
                   static_cast<long long>(phase.present_ms));
      std::fclose(pf);
    }
  }
  showcase_mark("present-ok");
  std::fprintf(stderr,
               "atmosphere-showcase: presented %d frames %ux%u "
               "present_ms=%.2f ms/p=%.2f "
               "phase mesh=%lld sync=%lld rebuild=%lld(n=%d) ocean=%lld "
               "record=%lld swap=%lld\n",
               presents, kW, kH, present_ms,
               present_count > 0 ? present_ms / present_count : 0.0,
               static_cast<long long>(phase.mesh_ms),
               static_cast<long long>(phase.sync_ms),
               static_cast<long long>(phase.rebuild_ms), phase.rebuild_count,
               static_cast<long long>(phase.ocean_prep_ms),
               static_cast<long long>(phase.record_ms),
               static_cast<long long>(phase.present_ms));

  bool bmp_signal_ok = !want_gpu || early_bmp_ok;
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
    const DWORD linger_start = GetTickCount();
    int linger_frames = 0;
    bool captured = early_bmp_ok;
    // Timed CI: do not sleep 33ms between presents (that capped ~30fps and
    // hid present cost). Interactive until-close still paces for readability.
    // SMT_ATMOSPHERE_SHOWCASE_PUMP_MS overrides (e.g. 16 ≈ 60Hz interactive).
    int pump_ms = linger.until_close ? 16 : 0;
    if (const char* pump_env = std::getenv("SMT_ATMOSPHERE_SHOWCASE_PUMP_MS");
        pump_env && *pump_env) {
      pump_ms = std::atoi(pump_env);
      if (pump_ms < 0) {
        pump_ms = 0;
      }
    }
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
      pump_messages(pump_ms);
    }
    if (!captured) {
      bmp_signal_ok = capture_showcase_bmp();
    }
    showcase_mark("linger-ok");
    const DWORD elapsed = GetTickCount() - linger_start;
    const DWORD wall_ms = elapsed == 0u ? 1u : elapsed;
    const float avg_fps =
        static_cast<float>(linger_frames) * 1000.f /
        static_cast<float>(wall_ms);
    std::fprintf(stderr,
                 "atmosphere-showcase: linger frames=%d wall_ms=%lu "
                 "avg_fps=%.2f last_fps=%.2f\n",
                 linger_frames, static_cast<unsigned long>(wall_ms), avg_fps,
                 cam->gpu().last_fps);
  } else if (want_gpu && !bmp_signal_ok) {
    bmp_signal_ok = capture_showcase_bmp();
  }

  cam->abandon_mesh();
  // Intentionally skip device->shutdown() — FlyCube DX12 teardown after a live
  // present has heap-corrupted ExitProcess (peer stormsurge / world3d). Leak
  // the Device* the same way MapViewport does after a live session.
  (void)owns_device;
  (void)device;
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

int atmosphere_showcase_body(Browser& browser, AtmosphereShowcaseMode mode) {
  return run_atmosphere_showcase_impl(browser, mode);
}

int run_atmosphere_showcase(Browser& browser, AtmosphereShowcaseMode mode) {
  const char* suite = nullptr;
  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      suite = "atmosphere.land";
      break;
    case AtmosphereShowcaseMode::kOcean:
      suite = "atmosphere.ocean";
      break;
    case AtmosphereShowcaseMode::kFull:
      suite = "atmosphere.full";
      break;
    case AtmosphereShowcaseMode::kCoast:
      suite = "atmosphere.coast";
      break;
    case AtmosphereShowcaseMode::kLegacy:
      // No interact.dsl twin yet — fall through to C++ body (china DEM parity).
      break;
    case AtmosphereShowcaseMode::kNone:
      break;
  }
  if (suite &&
      try_run_suite_script(browser, suite, detail::kAtmosphereShowcaseMarkLeaf,
                           /*clear_marks=*/true)) {
    return 0;
  }
  return atmosphere_showcase_body(browser, mode);
}

}  // namespace app
