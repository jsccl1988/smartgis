// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_ENGINE_H_
#define SCENIC_ENGINE_H_

#include <cstdint>

#include "scenic/scenic_export.h"

namespace scenic {

enum class Kind {
  kMap2d,
  kScene3d,
};

enum class GeomKind : uint8_t {
  kPoint = 0,
  kLine = 1,
  kPolygon = 2,
  kText = 3,
};

// Map-space vertex (Y already flipped in the host's map axis).
struct Vertex2 {
  float x = 0;
  float y = 0;
};

// One drawable feature. |xy| is non-owning and must stay valid through paint.
struct DrawItem {
  GeomKind kind = GeomKind::kPoint;
  uint32_t fill_colorref = 0;
  uint32_t stroke_colorref = 0;
  int stroke_width_px = 1;
  const Vertex2* xy = nullptr;
  uint32_t vertex_count = 0;
};

// View transform: view_x = map_x * scale + pan_x.
struct ViewXform {
  double pan_x = 0;
  double pan_y = 0;
  double scale = 1;
};

// Lon/lat framing + orbit for Scene3d software present (no HWND on the façade).
struct OrbitXform {
  float yaw = 0;
  float pitch = 0;
  float distance = 3.2f;
  double lon_min = 80.0;
  double lat_min = 16.0;
  double lon_max = 128.0;
  double lat_max = 52.0;
};

// Viewport size for a hosted Scenic session. Size-only — no HWND, no Device.
struct SessionDesc {
  uint32_t width_px = 0;
  uint32_t height_px = 0;
};

// Previous-generation map/scene engine. Public surface is namespace scenic
// only (POD DrawItem / ViewXform / OrbitXform). Copied rhi2d/rhi3d/scene3d
// types and leftover Smt_* are not this DLL's API.
class SCENIC_EXPORT Engine {
 public:
  virtual ~Engine() = default;
  virtual bool initialize(const SessionDesc& desc) = 0;
  virtual void shutdown() = 0;
  virtual bool present() = 0;
  virtual Kind kind() const = 0;
  virtual bool is_null() const { return false; }

  virtual void bind_view(const ViewXform& view) { (void)view; }
  virtual void bind_orbit(const OrbitXform& orbit) { (void)orbit; }
  virtual void bind_draw_items(const DrawItem* items, uint32_t count) {
    (void)items;
    (void)count;
  }
  // |hdc| is HDC. Product façade stays HWND-free.
  virtual bool paint_hdc(void* hdc, uint32_t width_px, uint32_t height_px) {
    (void)hdc;
    (void)width_px;
    (void)height_px;
    return false;
  }
  virtual bool export_bmp(const char* path, uint32_t width_px,
                          uint32_t height_px) {
    (void)path;
    (void)width_px;
    (void)height_px;
    return false;
  }
  virtual bool last_present_ok() const { return false; }
};

SCENIC_EXPORT Engine* create_null_engine(Kind kind);
SCENIC_EXPORT Engine* create_map2d_engine();
SCENIC_EXPORT Engine* create_scene3d_engine();

}  // namespace scenic

#endif  // SCENIC_ENGINE_H_
