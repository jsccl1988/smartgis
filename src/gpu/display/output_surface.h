// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_DISPLAY_OUTPUT_SURFACE_H_
#define GPU_DISPLAY_OUTPUT_SURFACE_H_

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "content/common/host_protocol.h"
#include "content/public/map_types.h"
#include "gpu/device/adapter_id.h"

namespace gpu {
namespace detail {

// Display output surface: DXGI shared texture, or a duplicated DIB.
// Pixels are DXGI_FORMAT_B8G8R8A8_UNORM. upload_bgra is the software present.
// Each surface is pinned to an AdapterId (GpuDeviceHub) for multi-GPU present.
class OutputSurface {
 public:
  OutputSurface() = default;
  ~OutputSurface();

  bool resize(uint32_t width_px,
              uint32_t height_px,
              content::PresentMode requested,
              HANDLE ui_process);
  void clear(uint8_t b, uint8_t g, uint8_t r, uint8_t a);
  // Software present of one finished BGRA buffer (DIB and DXGI when present).
  // Not a GL SwapBuffers.
  bool upload_bgra(const uint8_t* bgra, uint32_t stride_bytes);
  // Copy out BGRA8. Requires a software DIB (or a prior upload_bgra that kept bits).
  bool copy_bgra(uint8_t* dst, size_t dst_bytes) const;

  content::SharedHandleWire wire() const { return wire_; }
  // Local NT handle / DIB mapping. Attach on kSharedHandle; do not pickle.
  HANDLE share_handle() const { return local_handle_; }
  content::PresentMode mode() const { return mode_; }
  uint32_t generation() const { return wire_.generation; }
  bool has_d3d_device() const { return d3d_device_ != nullptr; }

  // Multi-GPU pin. Default invalid until GpuDeviceHub::bind_surface.
  AdapterId adapter_id() const { return adapter_id_; }
  void set_adapter_id(AdapterId id) { adapter_id_ = id; }

  // Bump wire generation so shell reattaches after device-lost / rebind.
  void bump_generation();

  // Narrow hooks for RHI import (M1). No FlyCube types.
  void* shared_texture_d3d11() const { return d3d_texture_; }
  void* d3d_device() const { return d3d_device_; }
  bool import_ready() const {
    return d3d_texture_ != nullptr && local_handle_ != nullptr;
  }

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
  AdapterId adapter_id_ = kAdapterInvalid;
};

}  // namespace detail
}  // namespace gpu

#endif  // GPU_DISPLAY_OUTPUT_SURFACE_H_
