// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_BACKEND_VIEW_PRESENT_MAP2D_FPS_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_BACKEND_VIEW_PRESENT_MAP2D_FPS_H_

namespace content {
class ShellBridge;
}  // namespace content

namespace content {
namespace detail {

// Optional HUD sample. No-op unless switch map2d-fps-bench-ms is a positive
// millisecond budget. Writes map2d-fps-bench.txt under the exe capture dir.
void run_map2d_fps_bench(content::ShellBridge& browser);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_BACKEND_VIEW_PRESENT_MAP2D_FPS_H_
