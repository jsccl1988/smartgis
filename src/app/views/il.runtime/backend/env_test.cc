// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/env.h"

#include "base/process/switches.h"

#include <cstdio>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_default_off_require_one() {
  base::clear_switches_for_test();
  app::detail::GpuEnvOpts opts;
  opts.gpu_env = "atmosphere-showcase-gpu";
  opts.gpu_policy = app::detail::GpuEnvPolicy::kDefaultOffRequireOne;
  opts.gpu_env_fallback = nullptr;
  expect(!app::detail::resolve_rhi_want_gpu(opts), "unset stays off");
  base::set_switch("atmosphere-showcase-gpu", "1");
  expect(app::detail::resolve_rhi_want_gpu(opts), "exact 1 is on");
  base::set_switch("atmosphere-showcase-gpu", "0");
  expect(!app::detail::resolve_rhi_want_gpu(opts), "0 is off");
  base::set_switch("atmosphere-showcase-gpu", "true");
  expect(!app::detail::resolve_rhi_want_gpu(opts), "true is not exact 1");
}

void test_default_on_unless_zero() {
  base::clear_switches_for_test();
  app::detail::GpuEnvOpts opts;
  opts.gpu_env = "plugin-mine-gpu";
  opts.gpu_policy = app::detail::GpuEnvPolicy::kDefaultOnUnlessZero;
  opts.gpu_env_fallback = "plugin-world3d-gpu";
  expect(app::detail::resolve_rhi_want_gpu(opts), "unset defaults on");
  base::set_switch("plugin-mine-gpu", "0");
  expect(!app::detail::resolve_rhi_want_gpu(opts), "primary 0 is off");
  base::clear_switch("plugin-mine-gpu");
  base::set_switch("plugin-world3d-gpu", "0");
  expect(!app::detail::resolve_rhi_want_gpu(opts), "fallback 0 is off");
  base::set_switch("plugin-mine-gpu", "2");
  expect(app::detail::resolve_rhi_want_gpu(opts), "non-zero primary is on");
}

}  // namespace

int main() {
  test_default_off_require_one();
  test_default_on_unless_zero();
  base::clear_switches_for_test();
  if (g_fails != 0) {
    std::fprintf(stderr, "capture_present_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::printf("capture_present_test: ok\n");
  return 0;
}
