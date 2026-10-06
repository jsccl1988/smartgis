// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_DIB_READ_H_
#define IL_RUNTIME_BACKEND_VIEW_DIB_READ_H_

#include "app/views/il.runtime/backend/view/pixel/gate.h"

#include <windows.h>

namespace app {
namespace detail {

// Client-area readback policy. Visible-signal checks only decide whether to
// retry and whether to fall back from PrintWindow to a screen blit. The BMP
// is written when GetDIBits fills the buffer.
struct CaptureOpts {
  int max_attempts = 1;
  DWORD pump_base_ms = 0;
  DWORD pump_step_ms = 0;
  bool require_shell_diversity = false;
  VisiblePolicy visible = VisiblePolicy::kGridLitFraction;
  // 0 = client size. When both >= 8, StretchBlt into this size before write.
  int dst_w = 0;
  int dst_h = 0;
};

// PrintWindow, then screen blit when the frame fails |opts|. Optional stretch.
bool capture_hwnd_bmp(HWND hwnd,
                      const wchar_t* filename,
                      const CaptureOpts& opts = {});

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_DIB_READ_H_
