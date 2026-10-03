// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/present/present_warmup.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/present/present_gpu_warmup.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"
#include "app/views/shell/harness/showcase/plugin/session/session_finish.h"

#include <cstdio>

namespace app {
namespace detail {
namespace {

struct PluginWarmupFail {
  content::Scene3dPresenter* cam = nullptr;
  PluginDeviceSession* session = nullptr;
  Browser* browser = nullptr;
  const char* fail_log_prefix = nullptr;
  PluginPresentFailPolicy on_fail;
};

void on_plugin_warmup_fail(int failed_frame, void* user) {
  auto* ctx = static_cast<PluginWarmupFail*>(user);
  if (!ctx) {
    return;
  }
  std::fprintf(stderr, "plugin-showcase: %s present_gpu failed frame %d\n",
               ctx->fail_log_prefix ? ctx->fail_log_prefix : "scene3d",
               failed_frame);
  PluginTeardownOpts teardown;
  teardown.clear_pointcloud = ctx->on_fail.clear_pointcloud;
  teardown.clear_tin = ctx->on_fail.clear_tin;
  teardown.abandon_mesh = ctx->on_fail.abandon_mesh;
  teardown.shutdown_device = ctx->on_fail.shutdown_device;
  teardown.destroy_owned_hwnd = true;
  teardown_plugin_device_session(ctx->cam, ctx->session, teardown);
  plugin_showcase_mark("present-fail");
  if (ctx->browser) {
    detach_maps(*ctx->browser);
  }
}

}  // namespace

int present_plugin_warmup_frames(content::Scene3dPresenter* cam,
                                 PluginDeviceSession* session,
                                 Browser& browser,
                                 const char* fail_log_prefix,
                                 const PluginPresentFailPolicy& on_fail,
                                 int frame_count) {
  if (!cam || !session || !session->device) {
    return 52;
  }
  PluginWarmupFail fail_ctx;
  fail_ctx.cam = cam;
  fail_ctx.session = session;
  fail_ctx.browser = &browser;
  fail_ctx.fail_log_prefix = fail_log_prefix;
  fail_ctx.on_fail = on_fail;

  PresentGpuWarmupOpts opts;
  opts.width_px = kPluginShowcasePresentW;
  opts.height_px = kPluginShowcasePresentH;
  opts.frames = frame_count;
  opts.pump_ms = 50;
  opts.on_fail = on_plugin_warmup_fail;
  opts.on_fail_user = &fail_ctx;
  if (const int rc = present_gpu_warmup(cam, session->device, opts)) {
    return rc;
  }
  plugin_showcase_mark("present-ok");
  return 0;
}

}  // namespace detail
}  // namespace app
