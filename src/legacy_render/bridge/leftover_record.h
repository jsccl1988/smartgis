// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_SCENE_LEFTOVER_RECORD_H_
#define RENDER_SCENE_LEFTOVER_RECORD_H_

#include <cstdint>
#include <vector>

#include "render/rhi/rhi.h"
#include "legacy_render/bridge/leftover_mesh.h"
#include "render/scene/scene.h"
#include "sdb/scene/scene.h"

namespace sdb {
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

  // HWND for FlyCube swapchain present when creating the preferred GPU device.
  void set_native_window(void* native_window);

  bool begin(uint32_t width, uint32_t height);
  bool record_world(const sdb::scene::World& world);
  bool record_map(const sdb::SmtMap* map);
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

}  // namespace scene
}  // namespace render

#endif  // RENDER_SCENE_LEFTOVER_RECORD_H_
