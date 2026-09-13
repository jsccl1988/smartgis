// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_SKIA_SKIA_H_
#define RENDER_SKIA_SKIA_H_

// Skia paint backend for Views chrome. Default is GDI (canvas.cc);
// optional real Skia via smt_has_skia + local pin (canvas_skia.cc).
// See docs/build/ui-views-skia.md.

#include "render/skia/canvas.h"
#include "render/skia/color.h"

namespace render {
namespace skia {

const char* module_id();

}  // namespace skia
}  // namespace render

#endif  // RENDER_SKIA_SKIA_H_
