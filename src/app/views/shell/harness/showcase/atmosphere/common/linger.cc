// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/common/linger.h"

#include "app/views/shell/harness/common/present/linger_policy.h"

namespace app {
namespace detail {

AtmosphereShowcaseLinger atmosphere_showcase_linger(bool want_gpu) {
  AtmosphereShowcaseLinger out;
  if (!want_gpu) {
    return out;
  }
  LingerEnvOpts opts;
  opts.timed_ms_env = "SMT_ATMOSPHERE_SHOWCASE_TIMED_MS";
  opts.linger_ms_env = "SMT_ATMOSPHERE_SHOWCASE_LINGER_MS";
  opts.linger_ms_zero_only = true;
  opts.default_until_close = true;
  const LingerPolicy parsed = parse_linger_env(opts);
  out.until_close = parsed.until_close;
  if (parsed.ms > 0) {
    out.ms = static_cast<DWORD>(parsed.ms);
  }
  return out;
}

}  // namespace detail
}  // namespace app
