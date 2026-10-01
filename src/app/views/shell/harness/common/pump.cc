// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/pump.h"

namespace app {
namespace detail {

void pump_messages(DWORD ms) {
  MSG msg;
  // ms==0: drain the queue once without Sleep (timed FPS linger / spin present).
  if (ms == 0) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    return;
  }
  const DWORD end = GetTickCount() + ms;
  while (GetTickCount() < end) {
    while (GetTickCount() < end &&
           PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
}

}  // namespace detail
}  // namespace app
