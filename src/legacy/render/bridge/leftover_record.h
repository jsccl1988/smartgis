// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_SCENE_LEFTOVER_RECORD_H_
#define RENDER_SCENE_LEFTOVER_RECORD_H_

#include <cstdint>
#include <vector>

#include "legacy/render/bridge/leftover_mesh.h"
#include "render/rhi/rhi.h"
#include "render/scene/scene.h"
#include "gis/world/scene.h"

namespace gis {
class SmtMap;
}

// Records leftover GDI 2D GIS and leftover GL VB/IB onto one Device/list.

namespace render {
namespace scene {

// Shared leftover 2D + 3D recorder. GIS goes through GpuScene; leftover
// SmtVertexBuffer / SmtIndexBuffer go through leftover_mesh on the same list.
class LeftoverRecorder {
 public:
  LeftoverRecorder();
  ~LeftoverRecorder();
  LeftoverRecorder(const LeftoverRecorder&) = delete;
  LeftoverRecorder& operator=(const LeftoverRecorder&) = delete;

  bool attach(render::rhi::Device* device);
  render::rhi::Device* device() const { return device_; }
  render::rhi::CommandList* list() const { return list_; }
  bool is_open() const { return open_; }

  // Bookkeeping HWND (MFC view). Does not create FlyCube — that HWND is
  // owned by SmtGdi/Gl present. FlyCube uses attach() on a dedicated HWND.
  void set_native_window(void* native_window);
  void* native_window() const { return native_window_; }

  // Strangler used by bind_rhi_present: store HWND and ensure an owned Null
  // Device for GpuScene / leftover_mesh recording without dual-owning present.
  bool bind_present_hwnd(void* native_window);

  bool begin(uint32_t width, uint32_t height);
  bool record_world(const gis::World& world);
  bool record_map(const gis::SmtMap* map);
  bool record_3d(render::SmtVertexBuffer* vb, render::SmtIndexBuffer* ib);
  bool finish();
  void release();

  GpuScene& gpu_scene() { return gpu_; }

 private:
  bool ensure_device();
  void destroy_list();
  void clear_leftover_3d();

  render::rhi::Device* device_;
  render::rhi::CommandList* list_;
  GpuScene gpu_;
  std::vector<LeftoverGpuMesh> leftover_3d_;
  void* native_window_;
  uint32_t width_;
  uint32_t height_;
  bool owned_device_;
  bool open_;
  bool pass_open_;
};

// Process-wide leftover session owned by SmtRender when that DLL is loaded.
// Plugin DLLs (GDI / GDI-simple / GL) resolve the same Device + CommandList.
LeftoverRecorder& leftover_session();
bool leftover_session_is_process_wide();

// Best-effort Null recording of one GIS map frame for leftover GDI paint.
// Safe on a GDI-shared HWND (no FlyCube). Returns false if recording skipped.
// Callers must invoke this AFTER GDI has drawn into the map buffer — running
// tessellate/GpuScene before BitBlt left the canvas white (gdi_map_paint_test).
bool leftover_record_map_frame(void* native_window, uint32_t width,
                               uint32_t height, const gis::SmtMap* map);

}  // namespace scene
}  // namespace render

#endif  // RENDER_SCENE_LEFTOVER_RECORD_H_
