// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/self_test/probe.h"

#include "app/views/shell/browser/browser.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/viewport/draw_host.h"

#include <cmath>
#include <cstdio>

namespace app {
namespace detail {

int self_test_present(Browser& browser) {
  // FlyCube orbit camera matrices must track shell yaw/pitch.
  ui::views::DrawHost* scene = browser.scene_draw_host();
  const float yaw_after = browser.orbit_frame()->yaw();
  const render::rhi::CameraMatrices cam =
      browser.orbit_frame()->camera_matrices(1.333f);
  if (cam.kind != render::rhi::CameraKind::kPerspective ||
      std::fabs(yaw_after - app::kScene3dDefaultYaw) < 0.001f) {
    self_test_detach_maps(browser);
    return 27;
  }
  // View matrix must not be identity after orbit.
  bool view_moved = false;
  for (int i = 0; i < 16; ++i) {
    const float ident = (i % 5 == 0) ? 1.f : 0.f;
    if (std::fabs(cam.view[i] - ident) > 1e-4f) {
      view_moved = true;
      break;
    }
  }
  if (!view_moved) {
    self_test_detach_maps(browser);
    return 28;
  }
  if (scene &&
      scene->attach_mode() == ui::views::DrawHost::AttachMode::kGpuPresent &&
      scene->rhi_device()) {
    // Optional atmosphere exercise: demo on for self-test only; normal
    // launches leave ocean/cloud disabled.
    browser.scene3d()->atmosphere_session().enable_demo();
    self_test_mark("atmosphere-demo");
    if (!browser.scene3d()->present_gpu(
            static_cast<render::rhi::Device*>(scene->rhi_device()), 64, 64)) {
      std::fprintf(stderr, "FlyCube present_gpu after orbit failed\n");
      self_test_detach_maps(browser);
      return 29;
    }
    self_test_mark("flycube-camera-ok");
  } else {
    self_test_mark("flycube-skipped");
  }
  return 0;
}

}  // namespace detail
}  // namespace app
