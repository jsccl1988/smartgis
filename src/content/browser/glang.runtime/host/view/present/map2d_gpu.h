// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_BACKEND_VIEW_PRESENT_MAP2D_GPU_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_BACKEND_VIEW_PRESENT_MAP2D_GPU_H_

namespace content {
class Map2dPresenter;
struct Shell;
}  // namespace content

namespace content {
namespace detail {

// Cold+warm present_gpu on a dedicated HWND when the map pane already owns
// a flip-model chain. Skips when no device can be created. Does not fail
// the BMP that already landed.
void run_map2d_showcase_gpu(content::Shell& browser,
                            content::Map2dPresenter* map2d,
                            int showcase_w,
                            int showcase_h);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_BACKEND_VIEW_PRESENT_MAP2D_GPU_H_
