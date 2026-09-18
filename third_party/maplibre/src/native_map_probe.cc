// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Compile-only probe of the fetched pin. Not linked into product chrome.
// Native mln::Map cannot be constructed here without HeadlessFrontend +
// a linked core (EGL/WGL, vendor, style codegen).

#if defined(SMT_HAS_NATIVE_MLN_MAP)
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <mln/map/map.hpp>
#include <mln/gfx/headless_frontend.hpp>

namespace maplibre_native_probe {

using NativeMap = mln::Map;
using NativeFrontend = mln::HeadlessFrontend;

int native_map_header_abi() {
  return static_cast<int>(sizeof(void*));
}

}  // namespace maplibre_native_probe
#endif
