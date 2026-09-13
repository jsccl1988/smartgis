// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "ui/xview/view_chrome.h"

#include "content/public/map_types.h"
#include "content/public/view_host.h"

namespace ui {

bool dispatch_chrome_message(content::ViewHost* host,
                             UINT message,
                             WPARAM wparam,
                             LPARAM lparam) {
  if (!host) {
    return false;
  }
  content::InputEvent e{};
  switch (message) {
    case WM_MOUSEMOVE:
      e.kind = content::InputEvent::Kind::kMouseMove;
      break;
    case WM_LBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kLDown;
      break;
    case WM_LBUTTONUP:
      e.kind = content::InputEvent::Kind::kLUp;
      break;
    case WM_LBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kLDClick;
      break;
    case WM_RBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kRDown;
      break;
    case WM_RBUTTONUP:
      e.kind = content::InputEvent::Kind::kRUp;
      break;
    case WM_RBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kRDClick;
      break;
    case WM_MOUSEWHEEL:
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = static_cast<int32_t>(GET_WHEEL_DELTA_WPARAM(wparam));
      break;
    case WM_KEYDOWN:
      e.kind = content::InputEvent::Kind::kKeyDown;
      e.key = static_cast<uint32_t>(wparam);
      return host->dispatch_input(e);
    default:
      return false;
  }
  e.x_px = static_cast<int32_t>(static_cast<short>(LOWORD(lparam)));
  e.y_px = static_cast<int32_t>(static_cast<short>(HIWORD(lparam)));
  e.flags = static_cast<uint32_t>(wparam);
  return host->dispatch_input(e);
}

}  // namespace ui
