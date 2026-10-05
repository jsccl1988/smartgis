// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_DEVICE_DEVICE_LOAD_H_
#define UI_VIEWS_MAP_DEVICE_DEVICE_LOAD_H_

#include <cstddef>
#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {
namespace detail {

using CreateRenderDeviceFn = int (*)(HINSTANCE, void*&);

// Legacy CreateRenderDevice export layout (vtable slots used by DrawHost).
struct DeviceVtable {
  void* dtor;
  int (*Init)(void* self, HWND hwnd, const char* logname);
  int (*Destroy)(void* self);
  int (*Release)(void* self);
  int (*Resize)(void* self, int orgx, int orgy, int cx, int cy);
};

struct DeviceObj {
  DeviceVtable* vtbl;
};

bool file_exists(const wchar_t* path);
void exe_dir(wchar_t* out, size_t cap);
HMODULE load_first(const wchar_t* const* names);
bool init_device_seh(void* device, HWND hwnd);

// Optional Scene3d swapchain downscale from SCENE3D_* env vars.
void clamp_scene3d_swapchain_size(uint32_t* w, uint32_t* h);

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_DEVICE_DEVICE_LOAD_H_
