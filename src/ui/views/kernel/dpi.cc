// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/dpi.h"

#include <cmath>

namespace ui {
namespace views {
namespace {

constexpr float kEpsilon = 0.0001f;

// Keep awareness enums local so we can GetProcAddress without requiring a
// particular SDK WINVER at every call site.
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((void*)(-4))
#endif

using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(void*);
using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);

constexpr int kProcessPerMonitorDpiAware = 2;  // PROCESS_PER_MONITOR_DPI_AWARE
constexpr int kMdtEffectiveDpi = 0;            // MDT_EFFECTIVE_DPI

GetDpiForWindowFn load_get_dpi_for_window() {
  static GetDpiForWindowFn fn = []() -> GetDpiForWindowFn {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) {
      return nullptr;
    }
    return reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(user32, "GetDpiForWindow"));
  }();
  return fn;
}

unsigned dpi_from_dc(HDC hdc) {
  if (!hdc) {
    return kDefaultDpi;
  }
  const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
  return dpi > 0 ? static_cast<unsigned>(dpi) : kDefaultDpi;
}

}  // namespace

float scale_factor_from_dpi(unsigned dpi) {
  if (dpi == 0) {
    return 1.f;
  }
  return static_cast<float>(dpi) / static_cast<float>(kDefaultDpi);
}

int dip_to_px(int dip, float scale_factor) {
  if (scale_factor <= kEpsilon) {
    return dip;
  }
  return static_cast<int>(std::lround(static_cast<double>(dip) * scale_factor));
}

int px_to_dip(int px, float scale_factor) {
  if (scale_factor <= kEpsilon) {
    return px;
  }
  return static_cast<int>(std::lround(static_cast<double>(px) / scale_factor));
}

unsigned dpi_for_hwnd(HWND hwnd) {
  if (hwnd && IsWindow(hwnd)) {
    if (GetDpiForWindowFn get_dpi = load_get_dpi_for_window()) {
      const UINT dpi = get_dpi(hwnd);
      if (dpi > 0) {
        return dpi;
      }
    }
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
      auto get_mon = reinterpret_cast<GetDpiForMonitorFn>(
          GetProcAddress(shcore, "GetDpiForMonitor"));
      if (get_mon) {
        HMONITOR mon =
            MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        UINT dpi_x = 0;
        UINT dpi_y = 0;
        if (SUCCEEDED(get_mon(mon, kMdtEffectiveDpi, &dpi_x, &dpi_y)) &&
            dpi_x > 0) {
          FreeLibrary(shcore);
          return dpi_x;
        }
      }
      FreeLibrary(shcore);
    }
    HDC hdc = GetDC(hwnd);
    const unsigned dpi = dpi_from_dc(hdc);
    if (hdc) {
      ReleaseDC(hwnd, hdc);
    }
    return dpi;
  }

  HDC screen = GetDC(nullptr);
  const unsigned dpi = dpi_from_dc(screen);
  if (screen) {
    ReleaseDC(nullptr, screen);
  }
  return dpi;
}

void clamp_rect_to_work_area(int* x, int* y, int width, int height, HWND anchor) {
  if (!x || !y || width <= 0 || height <= 0) {
    return;
  }
  POINT pt = {*x + width / 2, *y + height / 2};
  HMONITOR mon = nullptr;
  if (anchor && IsWindow(anchor)) {
    mon = MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST);
  } else {
    mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  }
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (!mon || !GetMonitorInfoW(mon, &mi)) {
    return;
  }
  const RECT& work = mi.rcWork;
  if (*x + width > work.right) {
    *x = work.right - width;
  }
  if (*y + height > work.bottom) {
    *y = work.bottom - height;
  }
  if (*x < work.left) {
    *x = work.left;
  }
  if (*y < work.top) {
    *y = work.top;
  }
}

bool enable_process_dpi_awareness() {
  static int state = 0;  // 0 = unset, 1 = ok, -1 = failed
  if (state != 0) {
    return state > 0;
  }

  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (user32) {
    auto set_ctx = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (set_ctx && set_ctx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
      state = 1;
      return true;
    }
  }

  HMODULE shcore = LoadLibraryW(L"shcore.dll");
  if (shcore) {
    auto set_awareness = reinterpret_cast<SetProcessDpiAwarenessFn>(
        GetProcAddress(shcore, "SetProcessDpiAwareness"));
    if (set_awareness &&
        SUCCEEDED(set_awareness(kProcessPerMonitorDpiAware))) {
      FreeLibrary(shcore);
      state = 1;
      return true;
    }
    FreeLibrary(shcore);
  }

  if (SetProcessDPIAware()) {
    state = 1;
    return true;
  }
  state = -1;
  return false;
}

}  // namespace views
}  // namespace ui
