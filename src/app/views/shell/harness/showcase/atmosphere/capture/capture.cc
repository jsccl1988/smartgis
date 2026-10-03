// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/capture/capture.h"

#include "app/views/shell/harness/common/capture/scene3d_capture.h"
#include "app/views/shell/harness/showcase/atmosphere/session/device_session.h"
#include "app/views/shell/harness/showcase/atmosphere/capture/label_composite.h"
#include "app/views/shell/harness/showcase/atmosphere/common/progress.h"

#include <cstdio>

namespace app {
namespace detail {
namespace {

struct AtmosphereLabelHook {
  AtmosphereShowcaseMode mode;
  content::Scene3dPresenter* cam = nullptr;
};

bool atmosphere_after_ok(const wchar_t* bmp_path, int w, int h, void* user) {
  auto* hook = static_cast<AtmosphereLabelHook*>(user);
  if (!hook || hook->mode != AtmosphereShowcaseMode::kLegacy) {
    return false;
  }
  if (composite_legacy_labels_onto_bmp(hook->cam, bmp_path, w, h)) {
    atmosphere_showcase_mark("labels-bmp-ok");
    return true;
  }
  atmosphere_showcase_mark("labels-bmp-skip");
  return false;
}

void atmosphere_mark(const char* step) {
  atmosphere_showcase_mark(step);
}

}  // namespace

bool capture_atmosphere_showcase_bmp(AtmosphereShowcaseMode mode,
                                     const char* mode_name,
                                     content::Scene3dPresenter* cam,
                                     render::rhi::Device* device,
                                     HWND present_hwnd,
                                     HWND owned_present_hwnd,
                                     bool want_gpu,
                                     bool globe_flythrough) {
  if (!cam || !device || !mode_name) {
    return !want_gpu;
  }
  wchar_t file[64] = {};
  swprintf_s(file, L"atmosphere-showcase-%S.bmp", mode_name);

  AtmosphereLabelHook hook{mode, cam};
  Scene3dHwndCaptureOpts opts;
  opts.bmp_leaf = file;
  opts.present_w = kAtmosphereShowcaseW;
  opts.present_h = kAtmosphereShowcaseH;
  opts.pre_capture_pump_ms = 80;
  opts.sleep_instead_of_pump = globe_flythrough;
  opts.skip_when_null_gpu = false;
  opts.require_color_diversity = true;
  opts.mark = atmosphere_mark;
  opts.log_prefix = "atmosphere-showcase";
  opts.after_ok = atmosphere_after_ok;
  opts.after_ok_user = &hook;

  HWND capture_hwnd =
      owned_present_hwnd ? owned_present_hwnd : present_hwnd;
  const bool ok = capture_scene3d_hwnd_bmp(cam, device, capture_hwnd, want_gpu,
                                           opts);
  if (!ok && want_gpu) {
    std::fprintf(stderr, "atmosphere-showcase: BMP lacks visible signal\n");
  }
  return ok;
}

}  // namespace detail
}  // namespace app
