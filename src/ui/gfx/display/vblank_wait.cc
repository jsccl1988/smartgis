// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/display/vblank_wait.h"

#include <dxgi.h>

namespace ui {
namespace gfx {
namespace {

HMONITOR monitor_for_hwnd(HWND hwnd) {
  if (hwnd) {
    return MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  }
  return MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
}

}  // namespace

VblankClock::~VblankClock() {
  reset();
}

void VblankClock::set_hwnd(HWND hwnd) {
  if (hwnd_ == hwnd && output_) {
    const HMONITOR now = monitor_for_hwnd(hwnd);
    if (now == monitor_) {
      return;
    }
  }
  hwnd_ = hwnd;
  reset();
}

void VblankClock::reset() {
  if (output_) {
    output_->Release();
    output_ = nullptr;
  }
  monitor_ = nullptr;
}

bool VblankClock::ensure_output() {
  const HMONITOR want = monitor_for_hwnd(hwnd_);
  if (output_ && monitor_ == want) {
    return true;
  }
  reset();
  monitor_ = want;

  IDXGIFactory1* factory = nullptr;
  if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                reinterpret_cast<void**>(&factory))) ||
      !factory) {
    return false;
  }

  for (UINT ai = 0; !output_; ++ai) {
    IDXGIAdapter1* adapter = nullptr;
    if (factory->EnumAdapters1(ai, &adapter) == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (!adapter) {
      continue;
    }
    for (UINT oi = 0; !output_; ++oi) {
      IDXGIOutput* output = nullptr;
      if (adapter->EnumOutputs(oi, &output) == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      if (!output) {
        continue;
      }
      DXGI_OUTPUT_DESC desc = {};
      if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == monitor_ &&
          desc.AttachedToDesktop) {
        output_ = output;
      } else {
        output->Release();
      }
    }
    adapter->Release();
  }
  factory->Release();
  return output_ != nullptr;
}

bool VblankClock::wait_next(uint32_t fallback_ms) {
  // fallback_ms == 0: return immediately on DXGI failure (caller already paced).
  const uint32_t sleep_ms = fallback_ms;
  if (!ensure_output()) {
    if (sleep_ms != 0) {
      ::Sleep(sleep_ms);
    }
    return false;
  }
  const HRESULT hr = output_->WaitForVBlank();
  if (FAILED(hr)) {
    reset();
    if (sleep_ms != 0) {
      ::Sleep(sleep_ms);
    }
    return false;
  }
  return true;
}

}  // namespace gfx
}  // namespace ui
