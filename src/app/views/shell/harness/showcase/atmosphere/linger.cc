// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/linger.h"

#include <cstdlib>
#include <cstring>

namespace app {
namespace detail {

AtmosphereShowcaseLinger atmosphere_showcase_linger(bool want_gpu) {
  AtmosphereShowcaseLinger out;
  if (!want_gpu) {
    return out;
  }
  if (const char* timed = std::getenv("SMT_ATMOSPHERE_SHOWCASE_TIMED_MS")) {
    const int v = std::atoi(timed);
    if (v > 0) {
      out.ms = static_cast<DWORD>(v);
      return out;
    }
  }
  if (const char* env = std::getenv("SMT_ATMOSPHERE_SHOWCASE_LINGER_MS")) {
    // Only "0" is honored; positive values are ignored so leftover shell env
    // cannot force a flash-and-exit.
    if (std::strcmp(env, "0") == 0) {
      return out;
    }
  }
  out.until_close = true;
  return out;
}

}  // namespace detail
}  // namespace app
