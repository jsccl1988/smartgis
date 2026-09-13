// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_PRESENT_H
#define GPU_PRESENT_H

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "content/public/host_protocol.h"
#include "content/public/map_types.h"

namespace gpu {
namespace detail {

// DXGI shared texture when the device exists; otherwise a duplicated DIB
// section. Pixels are DXGI_FORMAT_B8G8R8A8_UNORM.
class PresentTarget {
 public:
  PresentTarget() = default;
  ~PresentTarget();

  bool resize(uint32_t width_px,
              uint32_t height_px,
              content::PresentMode requested,
              HANDLE ui_process);
  void paint_clear(uint8_t b, uint8_t g, uint8_t r, uint8_t a);
  void copy_from_hwnd(HWND hwnd);

  content::SharedHandleWire wire() const { return wire_; }
  content::PresentMode mode() const { return mode_; }
  uint32_t generation() const { return wire_.generation; }

 private:
  void release();
  bool create_dxgi(uint32_t w, uint32_t h, HANDLE ui_process);
  bool create_dib(uint32_t w, uint32_t h, HANDLE ui_process);

  content::PresentMode mode_ = content::PresentMode::kSoftwareDib;
  content::SharedHandleWire wire_{};
  HANDLE local_handle_ = nullptr;
  void* bits_ = nullptr;
  void* d3d_device_ = nullptr;
  void* d3d_context_ = nullptr;
  void* d3d_texture_ = nullptr;
};

}  // namespace detail
}  // namespace gpu

#endif  // GPU_PRESENT_H
