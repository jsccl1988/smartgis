// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"

#include <algorithm>
#include <cmath>

#include "base/core/log.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/frame/terrain_mesh.h"
#include "effect/atmosphere/frame/atmosphere_effects.h"
#include "effect/scene/opaque_effect.h"
#include "effect/scene/scene.h"
#include "gis/vista/domain/atmosphere/systems/environment.h"
#include "render/graph/frame_graph.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "base/trace/event/process_trace.h"

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
  // DEM drape is the China raster. A 2D extent in pixels, or a world box,
  // normalizes that raster into a sticker on a huge ocean (flash-correct,
  // then 错位). Only a lon/lat box inside China may reframe the orbit.
  if (orbit_) {
    const Extent2 e = orbit_->world_extent();
    if (extent_looks_like_china(e)) {
      return e;
    }
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
  // White tint: draped hypsometric / china_rs keep authored RGB (olive
  // multiply made land read as flat mud under FlyCube textured PS).
  gpu_scene_.set_solid_color(1.f, 1.f, 1.f, 1.f);
  gpu_scene_.set_wireframe(wireframe_enabled_);
  // Drive DEM Lambert from atmosphere sun (azimuth / elevation scrub with time).
  {
    render::programs::Light light{};
    if (const gis::atmosphere::Environment* env_light =
            atmosphere.environment()) {
      const float az = env_light->params().sun_azimuth_rad;
      const float el = env_light->params().sun_elevation_rad;
      const float cos_el = std::cos(el);
      light.dir_x = std::cos(az) * cos_el;
      light.dir_y = -std::sin(el);
      light.dir_z = std::sin(az) * cos_el;
      // Stronger key + softer fill so sun scrub reads on DEM Lambert.
      light.ambient = 0.22f;
      light.intensity = 1.65f;
    }
    gpu_scene_.set_light(light);
  }
  {
    size_t tex_nodes = 0;
    for (size_t i = 0; i < terrain_world_.node_count(); ++i) {
      const gis::Node* n = terrain_world_.node_at(i);
      if (n && !n->terrain_rgba.empty() && n->terrain_tex_w > 0 &&
          n->terrain_tex_h > 0) {
        ++tex_nodes;
      }
    }
    LOGGING(LOG_INFO,
            "scene3d.present dem nodes=%zu textured=%zu size=%ux%u "
            "yaw=%.2f pitch=%.2f dist=%.2f",
            terrain_world_.node_count(), tex_nodes, width_px, height_px, yaw(),
            pitch(), distance());
    if (!local_xyz_.empty() && local_xyz_.size() >= 3) {
      float mn_x = local_xyz_[0];
      float mn_y = local_xyz_[1];
      float mn_z = local_xyz_[2];
      float mx_x = mn_x;
      float mx_y = mn_y;
      float mx_z = mn_z;
      for (size_t i = 0; i + 2 < local_xyz_.size(); i += 3) {
        mn_x = (std::min)(mn_x, local_xyz_[i]);
        mn_y = (std::min)(mn_y, local_xyz_[i + 1]);
        mn_z = (std::min)(mn_z, local_xyz_[i + 2]);
        mx_x = (std::max)(mx_x, local_xyz_[i]);
        mx_y = (std::max)(mx_y, local_xyz_[i + 1]);
        mx_z = (std::max)(mx_z, local_xyz_[i + 2]);
      }
      LOGGING(LOG_INFO,
              "scene3d.present mesh_aabb x=[%.2f,%.2f] y=[%.2f,%.2f] "
              "z=[%.2f,%.2f] verts=%zu",
              mn_x, mx_x, mn_y, mx_y, mn_z, mx_z, local_xyz_.size() / 3);
    }
  }
  float aspect = static_cast<float>(width_px) /
                 static_cast<float>(height_px > 0 ? height_px : 1);
  const render::rhi::CameraMatrices cam = camera_matrices(aspect);
  const render::rhi::CameraMatrices* view_camera = nullptr;
  if (device->backend() != render::rhi::Backend::kNull) {
    view_camera = &cam;
  } else {
    gpu_scene_.clear_view_camera();
  }
  render_engine_name = render::rhi::backend_display_name(device->backend());
  note_present_frame();

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
  // This HWND is the swapchain. Parent chrome is painted on the widget, not
  // here. A fullscreen shell quad replaces the terrain a frame later
  // (correct flash, then a shifted / flat cover). Do not composite it.
  (void)shell;
  (void)shell_generation;
  shell_overlay_.clear();
  render::graph::ViewInput view;
  view.width_px = width_px;
  view.height_px = height_px;
  view.camera = view_camera;
  view.effects.push_back(atmosphere_effects.pre_effect());
  view.effects.push_back(&opaque);
  view.effects.push_back(atmosphere_effects.post_effect());
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
