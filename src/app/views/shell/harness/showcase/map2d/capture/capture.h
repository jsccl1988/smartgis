// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_CAPTURE_CAPTURE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_CAPTURE_CAPTURE_H_

#include <windows.h>

namespace content {
class Map2dPresenter;
}  // namespace content

namespace app {
namespace detail {

// Resolved capture sidecar paths for one showcase mode leaf.
struct Map2dCapturePaths {
  wchar_t bmp_w[MAX_PATH] = {};
  char bmp_a[MAX_PATH] = {};
};

// Builds map2d-showcase-<name>.bmp under the exe capture dir and clears any
// prior leaf. Returns 0 on success, else 56.
int prepare_map2d_capture_paths(const char* mode_name, Map2dCapturePaths* out);

// Timed software export_bmp + phase logs. Optional SMT_MAP2D_EXPORT_REUSE=1
// warms present-cache off-clock. Returns 0 / 56.
int export_map2d_showcase_bmp(content::Map2dPresenter* map2d,
                              const Map2dCapturePaths& paths,
                              int showcase_w,
                              int showcase_h);

// Visible-signal gate + durable .keep.bmp sibling. Returns 0 / 54.
int verify_map2d_showcase_bmp(const char* mode_name,
                              const Map2dCapturePaths& paths);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_CAPTURE_CAPTURE_H_
