// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_CAPABILITY_EXPORT_FRAME_H_
#define APP_VIEWS_RUNTIME_CAPABILITY_EXPORT_FRAME_H_

#include "content/browser/capability/host.h"

namespace app {

class Browser;

namespace detail {

// Map2d / Scene3d BMP export (GPU present, then software fallback).
void bind_export(Browser& browser, content::CapabilityHost* out);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_CAPABILITY_EXPORT_FRAME_H_
