// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/horizon/atom/pump.h"

namespace app {
namespace detail {
namespace {

bool dispatch_pumped(MSG* msg) {
  if (msg->message == WM_QUIT) {
    // PeekMessage removes WM_QUIT. Put it back so the outer loop can exit.
    PostQuitMessage(static_cast<int>(msg->wParam));
    return false;
  }
  TranslateMessage(msg);
  DispatchMessageW(msg);
  return true;
}

}  // namespace

void pump_messages(DWORD ms) {
  MSG msg;
  // ms==0: drain the queue once without Sleep (timed FPS linger / spin present).
  if (ms == 0) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (!dispatch_pumped(&msg)) {
        return;
      }
    }
    return;
  }
  // Elapsed subtraction stays valid across the GetTickCount 49-day wrap.
  const DWORD t0 = GetTickCount();
  while (GetTickCount() - t0 < ms) {
    while (GetTickCount() - t0 < ms &&
           PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (!dispatch_pumped(&msg)) {
        return;
      }
    }
    if (GetTickCount() - t0 >= ms) {
      break;
    }
    const DWORD left = ms - (GetTickCount() - t0);
    Sleep(left < 10 ? left : 10);
  }
}

}  // namespace detail

void pump_views_messages(DWORD ms) { detail::pump_messages(ms); }

}  // namespace app
