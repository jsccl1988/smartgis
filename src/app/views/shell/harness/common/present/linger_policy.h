// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_COMMON_PRESENT_LINGER_POLICY_H_
#define APP_VIEWS_SHELL_HARNESS_COMMON_PRESENT_LINGER_POLICY_H_

namespace app {
namespace detail {

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

#endif  // APP_VIEWS_SHELL_HARNESS_COMMON_PRESENT_LINGER_POLICY_H_
