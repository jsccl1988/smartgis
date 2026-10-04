// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/device/device_load.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "base/process/switches.h"

namespace ui {
namespace views {
namespace detail {

bool file_exists(const wchar_t* path) {
  const DWORD attr = GetFileAttributesW(path);
  return attr != INVALID_FILE_ATTRIBUTES &&
         !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Optional Scene3d swapchain downscale. A buffer smaller than the HWND client
// (e.g. forced 640x480 on a ~2k child) clears navy on-screen even when
// DrawIndexed succeeds — DXGI does not stretch that path the way a matching
// showcase HWND does. Default: native client size. Opt in:
// SMT_SCENE3D_SWAPCHAIN_MAX=1280x720 (or WIDTHxHEIGHT).
void clamp_scene3d_swapchain_size(uint32_t* w, uint32_t* h) {
  if (!w || !h) {
    return;
  }
  // Showcase-sized present: interactive multi-k clients clear navy with
  // DrawIndexed ok; 640x480 top-level matches the land-PASS atmosphere shot.
  if (const char* force = base::switch_cstr("scene3d-force-640")) {
    if (force[0] == '1' && force[1] == '\0') {
      *w = 640;
      *h = 480;
      return;
    }
  }
  const char* spec = base::switch_cstr("scene3d-swapchain-max");
  if (!spec || !spec[0]) {
    return;
  }
  unsigned max_w = 0;
  unsigned max_h = 0;
  if (std::sscanf(spec, "%ux%u", &max_w, &max_h) != 2 || max_w < 64u ||
      max_h < 64u) {
    return;
  }
  if (*w <= max_w && *h <= max_h) {
    return;
  }
  const float scale =
      (std::min)(static_cast<float>(max_w) / static_cast<float>(*w),
                 static_cast<float>(max_h) / static_cast<float>(*h));
  *w = (std::max)(64u, static_cast<uint32_t>(*w * scale));
  *h = (std::max)(64u, static_cast<uint32_t>(*h * scale));
}

void exe_dir(wchar_t* out, size_t cap) {
  GetModuleFileNameW(nullptr, out, static_cast<DWORD>(cap));
  wchar_t* slash = wcsrchr(out, L'\\');
  if (slash) {
    slash[1] = 0;
  }
}

HMODULE load_first(const wchar_t* const* names) {
  for (size_t i = 0; names[i]; ++i) {
    if (HMODULE already = GetModuleHandleW(names[i])) {
      return already;
    }
    if (HMODULE mod = LoadLibraryW(names[i])) {
      return mod;
    }
  }
  return nullptr;
}

bool init_device_seh(void* device, HWND hwnd) {
  auto* obj = static_cast<DeviceObj*>(device);
  if (!obj || !obj->vtbl || !obj->vtbl->Init) {
    return false;
  }
  __try {
    return obj->vtbl->Init(obj, hwnd, "SmartGisViews") == 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

}  // namespace detail
}  // namespace views
}  // namespace ui
