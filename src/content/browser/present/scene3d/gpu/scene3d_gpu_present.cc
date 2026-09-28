// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"

#include "base/core/log.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/frame/terrain_mesh.h"
#include "effect/atmosphere/frame/atmosphere_effects.h"
#include "effect/scene/opaque_effect.h"
#include "effect/scene/scene.h"
#include "render/graph/frame_graph.h"
#include "render/rhi/rhi.h"
#include "base/trace/process_trace.h"

namespace content {

void Scene3dGpuPresent::bind_orbit(const OrbitFrame* orbit) {
  orbit_ = orbit;
}

void Scene3dGpuPresent::bind_map(const MapScene* scene) {
  scene_ = scene;
  local_xyz_.clear();
  local_idx_.clear();
  terrain_lod_edge_ = 0;
}

void Scene3dGpuPresent::set_wireframe_enabled(bool on) {
  wireframe_enabled_ = on;
  gpu_scene_.set_wireframe(on);
}

float Scene3dGpuPresent::yaw() const {
  return orbit_ ? orbit_->yaw() : kScene3dDefaultYaw;
}

float Scene3dGpuPresent::pitch() const {
  return orbit_ ? orbit_->pitch() : 0.4f;
}

float Scene3dGpuPresent::distance() const {
  return orbit_ ? orbit_->distance() : 3.2f;
}

void Scene3dGpuPresent::remember_view_size(int width_px, int height_px) const {
  if (orbit_) {
    const_cast<OrbitFrame*>(orbit_)->remember_view_size(width_px, height_px);
  }
}

Extent2 Scene3dGpuPresent::world_extent() const {
  if (orbit_) {
    return orbit_->world_extent();
  }
  return kChinaLonLatExtent;
}

render::rhi::CameraMatrices Scene3dGpuPresent::camera_matrices(
    float aspect) const {
  if (!orbit_) {
    return {};
  }
  return orbit_->camera_matrices(aspect);
}

render::rhi::CameraMatrices Scene3dGpuPresent::camera_matrices_ortho(
    float width_px, float height_px) const {
  if (!orbit_) {
    return {};
  }
  return orbit_->camera_matrices_ortho(width_px, height_px);
}

void Scene3dGpuPresent::abandon(AtmosphereSession* atmosphere) {
  std::lock_guard<std::mutex> lock(present_mu_);
  if (atmosphere) {
    atmosphere->release_passes();
  }
  mesh_device_ = nullptr;
  // FlyCube / MapViewport may have shut down (or leaked) the Device already.
  // release() would destroy_pipeline/buffer on a dangling Device* — AV on
  // self-test teardown. Match GpuScene::~GpuScene and drop handles only.
  gpu_scene_.abandon();
}

void Scene3dGpuPresent::rebuild_local_mesh() {
  // Caller must hold present_mu_ (present / paint).
  BASE_TRACE_EVENT("mesh", "scene3d.mesh");
  rebuild_terrain_mesh(&terrain_world_, scene_, world_extent(), distance(),
                       &local_xyz_, &local_idx_, &geo_frame_,
                       &terrain_lod_edge_);
}

bool Scene3dGpuPresent::present(render::rhi::Device* device, uint32_t width_px,
                                uint32_t height_px,
                                const ui::gfx::ShellRaster* shell,
                                uint64_t shell_generation,
                                AtmosphereSession& atmosphere) {
  BASE_TRACE_EVENT("present", "scene3d.present");
  if (!device || width_px == 0 || height_px == 0) {
    LOGGING(LOG_ERROR, "scene3d.present fail: bad args device=%p size=%ux%u",
            device, width_px, height_px);
    return false;
  }
  std::lock_guard<std::mutex> lock(present_mu_);
  remember_view_size(static_cast<int>(width_px), static_cast<int>(height_px));
  rebuild_local_mesh();
  if (terrain_world_.node_count() == 0) {
    LOGGING(LOG_ERROR,
            "scene3d.present fail: empty terrain mesh (no DEM / extent). "
            "size=%ux%u orbit_dist=%.3f",
            width_px, height_px, distance());
    return false;
  }
  if (mesh_device_ != device) {
    atmosphere.release_passes();
    gpu_scene_.abandon();
    mesh_device_ = device;
  }
  gpu_scene_.sync_from(terrain_world_);
  // Lit DEM albedo: muted ground green (reads better under default LightCB).
  gpu_scene_.set_solid_color(0.42f, 0.58f, 0.34f, 1.f);
  gpu_scene_.set_wireframe(wireframe_enabled_);
  const float aspect = static_cast<float>(width_px) /
                       static_cast<float>(height_px > 0 ? height_px : 1);
  const render::rhi::CameraMatrices cam = camera_matrices(aspect);
  const render::rhi::CameraMatrices* view_camera = nullptr;
  if (device->backend() != render::rhi::Backend::kNull) {
    view_camera = &cam;
  } else {
    gpu_scene_.clear_view_camera();
  }
  render_engine_name = render::rhi::backend_display_name(device->backend());

  const gis::atmosphere::Environment* env = atmosphere.environment();
  const bool ocean_on = env && env->ocean_enabled();
  const bool cloud_on = env && env->cloud_enabled();
  const bool sky_on = env && env->sky_enabled();
  const bool fog_on = env && env->fog_enabled();
  atmosphere.frame().set_ocean_enabled(ocean_on);
  atmosphere.frame().set_cloud_enabled(cloud_on);
  atmosphere.frame().set_sky_enabled(sky_on);
  atmosphere.frame().set_fog_enabled(fog_on);
  if (!atmosphere.prepare_for_present()) {
    LOGGING(LOG_ERROR, "scene3d.present fail: atmosphere.prepare_for_present");
    return false;
  }
  const int cloud_quality = (env && cloud_on) ? env->params().quality : 1;
  effect::atmosphere::AtmosphereEffects atmosphere_effects(&atmosphere.frame(),
                                                           cloud_quality);
  effect::scene::OpaqueEffect opaque(&gpu_scene_);
  if (shell && shell->bgra && shell->width_px != 0 && shell->height_px != 0) {
    shell_overlay_.bind(*shell, shell_generation);
  } else {
    shell_overlay_.clear();
  }
  render::graph::ViewInput view;
  view.width_px = width_px;
  view.height_px = height_px;
  view.camera = view_camera;
  view.effects.push_back(atmosphere_effects.pre_effect());
  view.effects.push_back(&opaque);
  view.effects.push_back(atmosphere_effects.post_effect());
  if (shell_overlay_.has_shell()) {
    view.effects.push_back(&shell_overlay_);
  }
  const bool ok = render::graph::present(device, view);
  if (!ok) {
    LOGGING(LOG_ERROR,
            "scene3d.present fail: graph::present size=%ux%u nodes=%zu "
            "backend=%s",
            width_px, height_px, terrain_world_.node_count(),
            render_engine_name);
  }
  return ok;
}

}  // namespace content
