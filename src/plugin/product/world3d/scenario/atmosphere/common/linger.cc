// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/common/linger.h"

namespace plugin {
namespace detail {

AtmosphereShowcaseLinger atmosphere_showcase_linger(bool want_gpu) {
  AtmosphereShowcaseLinger out;
  // Capture-only default — no sticky ATMOSPHERE_SHOWCASE_LINGER_MS.
  // Panel/CI Run finishes after BMP; interactive linger is stage-2.
  (void)want_gpu;
  out.until_close = false;
  out.ms = 0;
  return out;
}

}  // namespace detail
}  // namespace plugin
