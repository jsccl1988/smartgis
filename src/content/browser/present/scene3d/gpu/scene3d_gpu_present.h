// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_GPU_SCENE3D_GPU_PRESENT_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_GPU_SCENE3D_GPU_PRESENT_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <mutex>
#include <vector>

#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/host/shell_overlay_effect.h"
#include "content/browser/present/scene3d/frame/orbit_geo_frame.h"
#include "content/public/map_types.h"
#include "effect/scene/scene.h"
#include "gis/vista/world/world.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/shell_raster.h"

namespace content {

class AtmosphereSession;
class MapScene;

// GPU present path for 3D: DEM mesh / GpuScene / shell overlay / present mutex.
class Scene3dGpuPresent {
 public:
  Scene3dGpuPresent() = default;
  ~Scene3dGpuPresent() = default;

  Scene3dGpuPresent(const Scene3dGpuPresent&) = delete;
  Scene3dGpuPresent& operator=(const Scene3dGpuPresent&) = delete;

  void bind_orbit(const OrbitFrame* orbit);
  void bind_map(const MapScene* scene);

  // Drop GPU mesh pointers without destroying Device-owned buffers.
  void abandon(AtmosphereSession* atmosphere);

  bool present(render::rhi::Device* device, uint32_t width_px,
               uint32_t height_px, const ui::gfx::ShellRaster* shell,
               uint64_t shell_generation, AtmosphereSession& atmosphere);

  void set_wireframe_enabled(bool on);
  bool wireframe_enabled() const { return wireframe_enabled_; }

  const OrbitFrame* orbit() const { return orbit_; }
  const MapScene* scene() const { return scene_; }

  Extent2 world_extent() const;
  OrbitGeoFrame& geo_frame() { return geo_frame_; }
  const OrbitGeoFrame& geo_frame() const { return geo_frame_; }

  std::mutex& mutex() const { return present_mu_; }

  const std::vector<float>& local_xyz() const { return local_xyz_; }
  const std::vector<unsigned>& local_idx() const { return local_idx_; }

  // Caller must hold mutex(). Used by software paint path.
  void rebuild_local_mesh();

  float yaw() const;
  float pitch() const;
  float distance() const;

  render::rhi::CameraMatrices camera_matrices(float aspect) const;
  render::rhi::CameraMatrices camera_matrices_ortho(float width_px,
                                                    float height_px) const;

  void remember_view_size(int width_px, int height_px) const;

  // Last present/paint sets this for HUD badge (shared with software painter).
  mutable const char* render_engine_name = "pending";

 private:
  const MapScene* scene_ = nullptr;
  const OrbitFrame* orbit_ = nullptr;

  gis::World terrain_world_;
  effect::scene::GpuScene gpu_scene_;
  std::vector<float> local_xyz_;
  std::vector<unsigned> local_idx_;
  mutable std::mutex present_mu_;

  render::rhi::Device* mesh_device_ = nullptr;
  bool wireframe_enabled_ = false;
  detail::ShellOverlayEffect shell_overlay_;
  OrbitGeoFrame geo_frame_;
  int terrain_lod_edge_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_GPU_SCENE3D_GPU_PRESENT_H_
