// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_SHOT_PAINT_H_
#define IL_RUNTIME_BACKEND_VIEW_SHOT_PAINT_H_

#include "content/browser/glang.runtime/host/view/pixel/bmp.h"

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace content {
namespace detail {

// Software hypsometric DIB write. One paint, no GPU fallback decision.
bool write_software_scene3d_bmp(content::Scene3dPresenter* cam,
                                const wchar_t* bmp_w,
                                int w = kCaptureW,
                                int h = kCaptureH);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_BACKEND_VIEW_SHOT_PAINT_H_
