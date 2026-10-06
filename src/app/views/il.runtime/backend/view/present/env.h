// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_PRESENT_ENV_H_
#define IL_RUNTIME_BACKEND_VIEW_PRESENT_ENV_H_

namespace app {
namespace detail {

// How GPU-on is resolved from process switches / environment.
enum class GpuEnvPolicy {
  // Atmosphere: on only when primary env is exactly "1"; else default off.
  kDefaultOffRequireOne,
  // Plugin: off only when env is exactly "0"; else fallback env; else on.
  kDefaultOnUnlessZero,
};

// Switch keys only — no HWND, Device, or Browser.
struct GpuEnvOpts {
  const char* gpu_env = nullptr;
  GpuEnvPolicy gpu_policy = GpuEnvPolicy::kDefaultOnUnlessZero;
  const char* gpu_env_fallback = "plugin-world3d-gpu";
};

bool resolve_rhi_want_gpu(const GpuEnvOpts& opts);

// Parsed linger intent only — does not run a linger pump/loop.
struct LingerPolicy {
  bool until_close = false;
  int ms = 0;
};

// UI vs atmosphere disagree on LINGER_MS: UI honors any atoi; atmosphere
// only treats exact "0" as disable-until-close (positive leftover env ignored).
struct LingerEnvOpts {
  const char* timed_ms_env = nullptr;
  const char* linger_ms_env = nullptr;
  bool linger_ms_zero_only = false;
  bool default_until_close = false;
};

LingerPolicy parse_linger_env(const LingerEnvOpts& opts);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_PRESENT_ENV_H_
