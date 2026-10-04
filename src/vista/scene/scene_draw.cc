// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/scene/scene.h"

#include <cstdint>
#include <vector>

#include "vista/scene/draw_pass.h"
#include "vista/scene/cull/frustum_camera.h"
#include "vista/scene/cull/prep_cull.h"
#include "vista/scene/cull/frustum_aabb.h"
#include "vista/scene/gpu_mesh.h"

namespace vista {

bool GpuScene::record_draws(render::rhi::Device* device,
                            render::rhi::CommandList* list, uint32_t width,
                            uint32_t height,
                            const render::rhi::CameraMatrices* bound_camera) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  if (!ensure_pipelines(device)) {
    return false;
  }
  if (meshes_dirty_ || upload_device_ != device || upload_width_ != width ||
      upload_height_ != height) {
    if (!rebuild_meshes(device, width, height)) {
      return false;
    }
  }

  render::rhi::RenderPassDesc pass;
  pass.clear_r = background_r_;
  pass.clear_g = background_g_;
  pass.clear_b = background_b_;
  pass.clear_a = background_a_;
  pass.width = width;
  pass.height = height;
  pass.load_op = color_load_op_;
  pass.enable_depth = enable_depth_;
  pass.depth_load_op = depth_load_op_;
  pass.depth_clear = 1.f;

  bool have_3d = false;
  for (const GpuInstance& inst : instances_) {
    if (inst.kind == vista::NodeKind::kModel ||
        inst.kind == vista::NodeKind::kTerrain ||
        inst.kind == vista::NodeKind::kPointCloud ||
        inst.kind == vista::NodeKind::kTileset) {
      have_3d = true;
    }
  }

  double minx = 0;
  double miny = 0;
  double maxx = 1;
  double maxy = 1;
  resolve_view_envelope(width, height, &minx, &miny, &maxx, &maxy);

  const bool external_camera = bound_camera != nullptr;
  bool pass_opened = color_load_op_ == render::rhi::ColorLoadOp::kLoad;
  const FrustumPlanes* cull_frustum = nullptr;
  FrustumPlanes cull_planes_storage;
  const float* cull_view = nullptr;
  const float* cull_proj = nullptr;
  if (external_camera) {
    cull_planes_storage = extract_frustum_planes(*bound_camera);
    cull_frustum = &cull_planes_storage;
    cull_view = bound_camera->view;
    cull_proj = bound_camera->proj;
  } else if (view_camera_set_) {
    list->bind_camera(view_camera_);
    cull_planes_storage = extract_frustum_planes(view_camera_);
    cull_frustum = &cull_planes_storage;
    cull_view = view_camera_.view;
    cull_proj = view_camera_.proj;
  } else {
    list->bind_camera(render::rhi::make_ortho_camera(
        static_cast<float>(minx), static_cast<float>(maxx),
        static_cast<float>(miny), static_cast<float>(maxy), -1.f, 1.f));
  }

  std::vector<uint8_t> mesh_visible;
  const std::vector<uint8_t>* visible_ptr = nullptr;
  if (cull_frustum && detail::frustum_cull_enabled()) {
    std::vector<MeshCullItem> cull_items;
    cull_items.reserve(meshes_.size());
    for (const GpuMesh& mesh : meshes_) {
      cull_items.push_back(as_cull_item(mesh));
    }
    detail::prep_cull_meshes(cull_items, cull_frustum, &mesh_visible, nullptr,
                             cull_view, cull_proj);
    visible_ptr = &mesh_visible;
  }

  auto rec = [&](vista::NodeKind kind, const FrustumPlanes* frustum,
                 const std::vector<uint8_t>* visible) {
    detail::record_kind(list, pass, width, height, meshes_, kind, &pass_opened,
                        solid_pipeline_, textured_pipeline_, lit_pipeline_,
                        lit_textured_pipeline_, light_, frustum, visible,
                        solid_terrain_forced_);
  };

  rec(vista::NodeKind::kRasterLayer, nullptr, nullptr);
  rec(vista::NodeKind::kVectorLayer, nullptr, nullptr);
  if (have_3d && !external_camera && !view_camera_set_) {
    const float aspect =
        static_cast<float>(width) / static_cast<float>(height);
    list->bind_camera(render::rhi::make_perspective_camera(0.785398f, aspect,
                                                           0.1f, 100.f));
  }
  rec(vista::NodeKind::kModel, cull_frustum, visible_ptr);
  rec(vista::NodeKind::kTerrain, cull_frustum, visible_ptr);
  rec(vista::NodeKind::kTileset, cull_frustum, visible_ptr);
  rec(vista::NodeKind::kPointCloud, cull_frustum, visible_ptr);

  if (meshes_.empty()) {
    list->begin_render_pass(pass);
    list->set_viewport(0, 0, static_cast<float>(width),
                       static_cast<float>(height), 0, 1);
    list->end_render_pass();
  }
  return true;
}

bool GpuScene::record(render::rhi::Device* device,
                      render::rhi::CommandList* list, uint32_t width,
                      uint32_t height) {
  if (!record_draws(device, list, width, height)) {
    return false;
  }
  list->close();
  return true;
}

}  // namespace vista
