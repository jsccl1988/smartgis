// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_MAP_TYPES_H
#define CONTENT_PUBLIC_MAP_TYPES_H

#include <cstdint>
#include <cstddef>

// Host ABI types for chrome (WebView2 / WinUI / Views). Chrome must not
// include gis_map.h or rd_renderdevice.h.
namespace content {

enum class ViewKind { kMapEdit, kMapData, kScene3d };

enum class PresentMode { kSharedTexture, kChildHwnd, kSoftwareDib };

struct Extent2 {
  double xmin;
  double ymin;
  double xmax;
  double ymax;
};

struct InputEvent {
  enum class Kind {
    kMouseMove,
    kWheel,
    kLDown,
    kLUp,
    kLDClick,
    kRDown,
    kRUp,
    kRDClick,
    kKeyDown,
    kTextCommit
  };
  Kind kind;
  uint32_t flags;
  int32_t x_px;
  int32_t y_px;
  int32_t wheel;
  uint32_t key;
  char32_t text[8];
  uint64_t t_qpc;
};

struct FeatureId {
  uint8_t bytes[32];
  uint8_t len;
};

// Latest shared pixels. nt_handle is valid in the chrome process
// (DuplicateHandle from gpu). After Resize, ignore until FrameReady.
// Try ID3D11Device::OpenSharedResource1 first; if that fails, MapViewOfFile
// (software DIB, DXGI_FORMAT_B8G8R8A8_UNORM tightly packed).
struct SharedSurface {
  uint32_t generation;
  void* nt_handle;
  uint32_t width_px;
  uint32_t height_px;
  uint32_t format;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_MAP_TYPES_H
