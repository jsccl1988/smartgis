// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/present/globe_present.h"

#include "plugin/product/world3d/scenario/atmosphere/capture/capture.h"
#include "plugin/product/world3d/scenario/atmosphere/session/device_session.h"
#include "plugin/product/world3d/scene/fly/globe_fly.h"
#include "plugin/product/world3d/scenario/atmosphere/common/progress.h"
#include "plugin/runtime/host/capability/shell.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "vista/pass/atmosphere/globe/globe_pass.h"
#include "ui/views/map/viewport/draw_host.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <windows.h>

namespace plugin {
namespace detail {

AtmosphereGlobeFlyResult run_atmosphere_globe_fly_presents(
    AtmosphereShowcaseMode mode,
    const char* mode_name,
    content::Scene3dPresenter* cam,
    content::OrbitFrame* orbit,
    render::rhi::Device* device,
    HWND present_hwnd,
    HWND owned_present_hwnd,
    ui::views::DrawHost* scene,
    float china_yaw,
    float china_pitch) {
  AtmosphereGlobeFlyResult out;
  if (!cam || !orbit || !device || !mode_name) {
    return out;
  }

  const vista::GlobePass* globe =
      &cam->atmosphere_session().globe_pass();
  content::AtmosphereSession* atm = &cam->atmosphere_session();
  atmosphere_mark("globe-flyin");
  uint32_t pw = kAtmosphereShowcaseW;
  uint32_t ph = kAtmosphereShowcaseH;
  atmosphere_hwnd_present_size(
      owned_present_hwnd ? owned_present_hwnd : present_hwnd, &pw, &ph);
  constexpr int kFlyFrames = 96;
  for (int i = 0; i < kFlyFrames; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(kFlyFrames - 1);
    plugin::apply_world3d_globe_flythrough(orbit, t, china_yaw, china_pitch, globe, atm);
    if (scene) {
      scene->request_frame();
    } else if (!cam->present_gpu(device, pw, ph)) {
      std::fprintf(stderr,
                   "atmosphere-showcase: globe fly-in present failed\n");
      break;
    }
    ++out.presents_added;
    Sleep(8);
  }
  atmosphere_mark("globe-high");
  plugin::apply_world3d_globe_flythrough(orbit, 0.48f, china_yaw, china_pitch, globe, atm);
  out.early_bmp_ok = capture_atmosphere_showcase_bmp(
      mode, mode_name, cam, device, present_hwnd, owned_present_hwnd,
      /*want_gpu=*/true, /*globe_flythrough=*/true, scene);

  // Qinling / central mountains (west-east skim), not the suite score BMP.
  plugin::apply_world3d_globe_flythrough(orbit, 0.76f, china_yaw, china_pitch, globe, atm);
  for (int warm = 0; warm < 4; ++warm) {
    if (scene) {
      scene->request_frame();
    } else {
      (void)cam->present_gpu(device, pw, ph);
    }
    Sleep(24);
  }
  if (capture_atmosphere_named_bmp(
          cam, device, present_hwnd, owned_present_hwnd,
          L"atmosphere-showcase-globe-near.bmp", /*globe_flythrough=*/true,
          scene)) {
    atmosphere_mark("globe-near-ok");
  } else {
    atmosphere_mark("globe-near-skip");
  }

  // East China Sea: distinct lon/lat + ocean on; warm several presents so the
  // DXGI buffer cannot be a stale copy of the mountain skim frame.
  plugin::apply_world3d_globe_flythrough(orbit, 1.f, china_yaw, china_pitch, globe, atm);
  if (atm) {
    atm->set_ocean_enabled(true);
    atm->set_sat_cloud_enabled(false);
  }
  for (int warm = 0; warm < 8; ++warm) {
    if (scene) {
      scene->request_frame();
    } else {
      (void)cam->present_gpu(device, pw, ph);
    }
    Sleep(28);
  }
  if (capture_atmosphere_named_bmp(
          cam, device, present_hwnd, owned_present_hwnd,
          L"atmosphere-showcase-globe-ocean.bmp", /*globe_flythrough=*/true,
          scene)) {
    atmosphere_mark("globe-ocean-ok");
  } else {
    atmosphere_mark("globe-ocean-skip");
  }

  // If near/ocean collided (same BitBlt), nudge yaw and recapture ocean once.
  {
    wchar_t near_path[MAX_PATH] = {};
    wchar_t ocean_path[MAX_PATH] = {};
    if (HarnessShell* shell = atmosphere_scenario_shell();
        shell &&
        shell->capture_path(near_path, MAX_PATH,
                            L"atmosphere-showcase-globe-near.bmp") &&
        shell->capture_path(ocean_path, MAX_PATH,
                            L"atmosphere-showcase-globe-ocean.bmp")) {
      FILE* a = nullptr;
      FILE* b = nullptr;
      bool same = false;
      if (_wfopen_s(&a, near_path, L"rb") == 0 && a &&
          _wfopen_s(&b, ocean_path, L"rb") == 0 && b) {
        same = true;
        unsigned char ba[4096];
        unsigned char bb[4096];
        for (;;) {
          const size_t na = std::fread(ba, 1, sizeof(ba), a);
          const size_t nb = std::fread(bb, 1, sizeof(bb), b);
          if (na != nb || (na > 0 && std::memcmp(ba, bb, na) != 0)) {
            same = false;
            break;
          }
          if (na == 0) {
            break;
          }
        }
        std::fclose(a);
        std::fclose(b);
      } else {
        if (a) {
          std::fclose(a);
        }
        if (b) {
          std::fclose(b);
        }
      }
      if (same) {
        atmosphere_mark("globe-ocean-dup");
        orbit->set_yaw(orbit->yaw() + 0.55f);
        orbit->set_pitch((std::max)(-0.15f, orbit->pitch() - 0.18f));
        orbit->set_distance(orbit->distance() * 0.92f);
        if (atm) {
          atm->set_ocean_enabled(true);
        }
        for (int warm = 0; warm < 6; ++warm) {
          if (scene) {
            scene->request_frame();
          } else {
            (void)cam->present_gpu(device, pw, ph);
          }
          Sleep(30);
        }
        if (capture_atmosphere_named_bmp(
                cam, device, present_hwnd, owned_present_hwnd,
                L"atmosphere-showcase-globe-ocean.bmp",
                /*globe_flythrough=*/true, scene)) {
          atmosphere_mark("globe-ocean-retry-ok");
        }
      }
    }
  }
  plugin::apply_world3d_globe_flythrough(orbit, 0.48f, china_yaw, china_pitch, globe, atm);
  return out;
}

}  // namespace detail
}  // namespace plugin
