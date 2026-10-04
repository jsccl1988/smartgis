// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/present/linger_policy.h"

#include <cstdlib>
#include <cstring>

#include "base/process/switches.h"

namespace app {
namespace detail {

LingerPolicy parse_linger_env(const LingerEnvOpts& opts) {
  LingerPolicy out;
  if (opts.timed_ms_env) {
    if (const char* timed = base::switch_cstr(opts.timed_ms_env)) {
      if (timed[0]) {
        const int v = std::atoi(timed);
        if (v > 0) {
          out.ms = v;
          return out;
        }
      }
    }
  }
  if (opts.linger_ms_env) {
    if (const char* env = base::switch_cstr(opts.linger_ms_env)) {
      if (opts.linger_ms_zero_only) {
        if (std::strcmp(env, "0") == 0) {
          return out;
        }
      } else if (env[0]) {
        out.ms = std::atoi(env);
        return out;
      }
    }
  }
  if (opts.default_until_close) {
    out.until_close = true;
  }
  return out;
}

}  // namespace detail
}  // namespace app
