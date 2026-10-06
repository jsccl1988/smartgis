// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/common/linger.h"

#include "app/views/il.runtime/backend/view/present/env.h"

namespace plugin {
namespace detail {

AtmosphereShowcaseLinger atmosphere_showcase_linger(bool want_gpu) {
  AtmosphereShowcaseLinger out;
  if (!want_gpu) {
    return out;
  }
  app::detail::LingerEnvOpts opts;
  opts.timed_ms_env = "atmosphere-showcase-timed-ms";
  opts.linger_ms_env = "atmosphere-showcase-linger-ms";
  opts.linger_ms_zero_only = true;
  opts.default_until_close = true;
  const app::detail::LingerPolicy parsed = app::detail::parse_linger_env(opts);
  out.until_close = parsed.until_close;
  if (parsed.ms > 0) {
    out.ms = static_cast<DWORD>(parsed.ms);
  }
  return out;
}

}  // namespace detail
}  // namespace plugin
