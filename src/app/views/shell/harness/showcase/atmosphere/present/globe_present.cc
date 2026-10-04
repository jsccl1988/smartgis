// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/present/globe_present.h"

#include "app/views/shell/harness/common/capture/bmp.h"
#include "app/views/shell/harness/showcase/atmosphere/capture/capture.h"
#include "app/views/shell/harness/showcase/atmosphere/session/device_session.h"
#include "app/views/shell/harness/showcase/atmosphere/fly/globe_fly.h"
#include "app/views/shell/harness/showcase/atmosphere/common/progress.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "vista/atmosphere/globe/globe_pass.h"
#include "render/rhi/rhi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include "base/process/switches.h"

namespace app {
namespace detail {
namespace {

bool harness_record_enabled() {
  const char* r = base::switch_cstr("harness-record");
  if (!r || !*r) {
    return false;
  }
  return (r[0] == '1' && r[1] == '\0') || std::strcmp(r, "true") == 0 ||
         std::strcmp(r, "yes") == 0 || std::strcmp(r, "on") == 0;
}

}  // namespace

AtmosphereGlobeFlyResult run_atmosphere_globe_fly_presents(
    AtmosphereShowcaseMode mode,
    const char* mode_name,
    content::Scene3dPresenter* cam,
    content::OrbitFrame* orbit,
    render::rhi::Device* device,
    HWND owned_present_hwnd,
    float china_yaw,
    float china_pitch) {
  AtmosphereGlobeFlyResult out;
  if (!cam || !orbit || !device || !mode_name) {
    return out;
  }

  const vista::GlobePass* globe =
      &cam->atmosphere_session().globe_pass();
  content::AtmosphereSession* atm = &cam->atmosphere_session();
  atmosphere_showcase_mark("globe-flyin");
  constexpr int kFlyFrames = 96;
  for (int i = 0; i < kFlyFrames; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(kFlyFrames - 1);
    apply_globe_flythrough(orbit, t, china_yaw, china_pitch, globe, atm);
    if (!cam->present_gpu(device, kAtmosphereShowcaseW, kAtmosphereShowcaseH)) {
      std::fprintf(stderr,
                   "atmosphere-showcase: globe fly-in present failed\n");
      break;
    }
    ++out.presents_added;
    // Do not DispatchMessage here: shell Map2d paint AVs under DX12 globe
    // fly (LayerStore::feature_count). GPU present does not need a pump.
    Sleep(8);
  }
  atmosphere_showcase_mark("globe-high");
  // High-altitude pose for the suite BMP / landish gates.
  apply_globe_flythrough(orbit, 0.48f, china_yaw, china_pitch, globe, atm);
  out.early_bmp_ok = capture_atmosphere_showcase_bmp(
      mode, mode_name, cam, device, owned_present_hwnd, owned_present_hwnd,
      /*want_gpu=*/true, /*globe_flythrough=*/true);

  // Keyframe dump AFTER the hot present loop — in-loop GDI capture AVs DX12.
  if (harness_record_enabled() && owned_present_hwnd) {
    constexpr float kKeys[] = {0.f,   0.10f, 0.22f, 0.36f, 0.48f,
                               0.62f, 0.74f, 0.86f, 0.94f, 1.f};
    for (float kt : kKeys) {
      apply_globe_flythrough(orbit, kt, china_yaw, china_pitch, globe, atm);
      if (!cam->present_gpu(device, kAtmosphereShowcaseW,
                            kAtmosphereShowcaseH)) {
        break;
      }
      Sleep(40);
      wchar_t path[MAX_PATH] = {};
      wchar_t leaf[96] = {};
      swprintf_s(leaf, L"record\\atmosphere_globe_fly\\frame_%05d.bmp",
                 out.dumped_frames);
      if (!exe_capture_path(path, MAX_PATH, leaf)) {
        continue;
      }
      if (capture_hwnd_bmp(owned_present_hwnd, path)) {
        ++out.dumped_frames;
      }
    }
    apply_globe_flythrough(orbit, 0.48f, china_yaw, china_pitch, globe, atm);
    atmosphere_showcase_mark("globe-frames-ok");
  }
  return out;
}

}  // namespace detail
}  // namespace app
