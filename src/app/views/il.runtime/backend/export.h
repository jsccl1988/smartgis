// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_EXPORT_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_EXPORT_H_

#include <string>

namespace app {

class Browser;

namespace detail {

// GPU present when live, then software hypsometric fallback to |bmp_w|.
bool export_scene3d_bmp(Browser* b, const wchar_t* bmp_w);

// Frames the document then software-exports Map2d to |bmp_w|.
struct Map2dExportOpts {
  bool apply_frame = true;
  bool invalidate_cache = true;
  bool kick_paint = true;
  int pump_ms = 200;
  bool require_features = true;
  int width = 0;
  int height = 0;
};

bool export_map2d_bmp(Browser* b,
                      const wchar_t* bmp_w,
                      const std::string& frame,
                      const Map2dExportOpts& opts = {});

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_EXPORT_H_
