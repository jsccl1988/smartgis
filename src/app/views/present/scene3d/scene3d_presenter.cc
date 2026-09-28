// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/present/scene3d/scene3d_presenter.h"

#include "app/views/camera/map_host_extent.h"
#include "app/views/camera/view_frame.h"
#include "app/views/document/map_scene.h"
#include "content/public/map_contents.h"
#include "gis/vista/world/terrain/dem_frame.h"
#include "gis/vista/world/terrain/dem_raster.h"
#include "gis/vista/world/terrain/land_mask.h"
#include "gis/vista/world/world.h"
#include "effect/atmosphere/frame/atmosphere_effects.h"
#include "render/graph/frame_graph.h"
#include "render/rhi/rhi.h"
#include "effect/scene/opaque_effect.h"
#include "effect/scene/scene.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace app {

Scene3dPresenter::Scene3dPresenter() {
  if (const char* env = std::getenv("SMT_SCENE3D_WIREFRAME")) {
    if (env[0] == '1' && env[1] == '\0') {
      wireframe_enabled_ = true;
    }
  }
  gpu_scene_.set_wireframe(wireframe_enabled_);
  atmosphere_frame_.set_ocean_pass(&ocean_pass_);
  atmosphere_frame_.set_cloud_pass(&cloud_pass_);
  atmosphere_frame_.set_sky_pass(&sky_pass_);
  atmosphere_frame_.set_fog_pass(&fog_pass_);
}

Scene3dPresenter::~Scene3dPresenter() {
  release_engine_logo_overlay();
  release_atmosphere_passes();
  release_mesh();
}

void Scene3dPresenter::set_wireframe_enabled(bool on) {
  wireframe_enabled_ = on;
  gpu_scene_.set_wireframe(on);
}

void Scene3dPresenter::set_render_engine_name(const char* name) const {
  render_engine_name_ = (name && name[0]) ? name : "unknown";
}


void Scene3dPresenter::bind_orbit(const OrbitFrame* orbit) {
  orbit_ = orbit;
}

void Scene3dPresenter::bind_label_frame(const ViewFrame* frame) {
  label_frame_ = frame;
}

void Scene3dPresenter::bind_map(const MapScene* scene) {
  scene_ = scene;
  local_xyz_.clear();
  local_idx_.clear();
  terrain_lod_edge_ = 0;
  abandon_mesh();
}

void Scene3dPresenter::bind_contents(content::MapContents* session,
                                      uint32_t view_id) {
  contents_ = session;
  view_id_ = view_id;
}

void Scene3dPresenter::abandon_mesh() {
  release_atmosphere_passes();
  mesh_device_ = nullptr;
  // FlyCube / MapViewport may have shut down (or leaked) the Device already.
  // release() would destroy_pipeline/buffer on a dangling Device* — AV on
  // self-test teardown. Match GpuScene::~GpuScene and drop handles only.
  gpu_scene_.abandon();
}


void Scene3dPresenter::release_mesh() {
  abandon_mesh();
}

void Scene3dPresenter::reset() {
  if (orbit_) {
    const_cast<OrbitFrame*>(orbit_)->reset();
  }
}


void Scene3dPresenter::remember_view_size(int width_px, int height_px) const {
  if (orbit_) {
    const_cast<OrbitFrame*>(orbit_)->remember_view_size(width_px, height_px);
  }
}


content::Extent2 Scene3dPresenter::world_extent() const {
  if (orbit_) {
    return orbit_->world_extent();
  }
  return kChinaLonLatExtent;
}


void Scene3dPresenter::apply_draft(const tool::Draft& draft) {
  if (draft.kind == tool::DraftKind::kKey) {
    uint32_t k = draft.key;
    if (k >= 'a' && k <= 'z') {
      k = k - ('a' - 'A');
    }
    if (k == 'K') {
      set_wireframe_enabled(!wireframe_enabled_);
      return;
    }
    if (k == 'J') {
      set_wireframe_enabled(false);
      return;
    }
  }
  if (orbit_) {
    const_cast<OrbitFrame*>(orbit_)->apply_draft(draft);
  }
}


render::rhi::CameraMatrices Scene3dPresenter::camera_matrices(
    float aspect) const {
  if (!orbit_) {
    return {};
  }
  return orbit_->camera_matrices(aspect);
}


render::rhi::CameraMatrices Scene3dPresenter::camera_matrices_ortho(
    float width_px, float height_px) const {
  if (!orbit_) {
    return {};
  }
  return orbit_->camera_matrices_ortho(width_px, height_px);
}


void Scene3dPresenter::rebuild_local_mesh() {
  const int lod_edge = gis::DemRaster::lod_max_edge(distance());
  const content::Extent2 extent = world_extent();
  if (!local_xyz_.empty() && !local_idx_.empty() &&
      terrain_lod_edge_ == lod_edge && geo_frame_.matches_extent(extent)) {
    return;
  }
  // Clear prior terrain nodes, then seed via GIS DEM API (no leftover DEM).
  while (terrain_world_.node_count() > 0) {
    const gis::Node* n = terrain_world_.node_at(0);
    if (!n || !terrain_world_.remove_node(n->id)) {
      break;
    }
  }
  local_xyz_.clear();
  local_idx_.clear();
  geo_frame_ = OrbitGeoFrame::from_extent(extent);
  std::vector<gis::LonLatRing> rings;
  if (scene_) {
    scene_->export_land_rings(&rings);
  }
  gis::Node* node = gis::seed_china_dem_into_world(
      &terrain_world_, rings.empty() ? nullptr : rings.data(), rings.size(),
      "views_dem", lod_edge);
  if (node && node->has_terrain_mesh()) {
    local_xyz_ = node->terrain_positions;
    local_idx_.assign(node->terrain_indices.begin(),
                      node->terrain_indices.end());
    // Same lon/lat → orbit frame as OrbitFrame::project_lon_lat and atmosphere.
    geo_frame_.capture_elev_center(local_xyz_);
    geo_frame_.normalize_xyz(&local_xyz_);
    std::vector<uint32_t> idx(local_idx_.begin(), local_idx_.end());
    terrain_world_.set_terrain_mesh(node->id, local_xyz_.data(),
                                    local_xyz_.size(), idx.data(), idx.size());
    terrain_lod_edge_ = lod_edge;
  }
}

bool Scene3dPresenter::present_gpu(render::rhi::Device* device,
                                    uint32_t width_px, uint32_t height_px,
                                    const ui::gfx::ShellRaster* shell,
                                    uint64_t shell_generation) {
  if (!device || width_px == 0 || height_px == 0) {
    return false;
  }
  remember_view_size(static_cast<int>(width_px), static_cast<int>(height_px));
  rebuild_local_mesh();
  if (terrain_world_.node_count() == 0) {
    return false;
  }
  if (mesh_device_ != device) {
    release_atmosphere_passes();
    // Previous Device may already be shut down / leaked; do not destroy_*.
    gpu_scene_.abandon();
    mesh_device_ = device;
  }
  gpu_scene_.sync_from(terrain_world_);
  // Terrain color comes from draped china_rs / hypsometric bake (textured).
  // Keep a mid SoT tint only as untextured fallback.
  gpu_scene_.set_solid_color(0.82f, 0.78f, 0.42f, 1.f);
  gpu_scene_.set_wireframe(wireframe_enabled_);
  const float aspect = static_cast<float>(width_px) /
                       static_cast<float>(height_px > 0 ? height_px : 1);
  const render::rhi::CameraMatrices cam = camera_matrices(aspect);
  // FlyCube binds this orbit camera. Null leaves the pointer null so
  // GpuScene does not frustum-cull (StubCommandList under lit+cull).
  const render::rhi::CameraMatrices* view_camera = nullptr;
  if (device->backend() != render::rhi::Backend::kNull) {
    view_camera = &cam;
  } else {
    gpu_scene_.clear_view_camera();
  }
  render_engine_name_ = render::rhi::backend_display_name(device->backend());

  const bool ocean_on = atmosphere_ && atmosphere_->ocean_enabled();
  const bool cloud_on = atmosphere_ && atmosphere_->cloud_enabled();
  const bool sky_on = atmosphere_ && atmosphere_->sky_enabled();
  const bool fog_on = atmosphere_ && atmosphere_->fog_enabled();
  atmosphere_frame_.set_ocean_enabled(ocean_on);
  atmosphere_frame_.set_cloud_enabled(cloud_on);
  atmosphere_frame_.set_sky_enabled(sky_on);
  atmosphere_frame_.set_fog_enabled(fog_on);
  if (!prepare_atmosphere_ocean() || !prepare_atmosphere_clouds() ||
      !prepare_atmosphere_sky() || !prepare_atmosphere_fog()) {
    return false;
  }
  const int cloud_quality =
      (atmosphere_ && cloud_on) ? atmosphere_->params().quality : 1;
  effect::atmosphere::AtmosphereEffects atmosphere_effects(&atmosphere_frame_,
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
  return render::graph::present(device, view);
}


}  // namespace app
