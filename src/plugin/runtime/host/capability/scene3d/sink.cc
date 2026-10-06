// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/capability/scene3d/sink.h"

namespace plugin {

void Scene3dSink::set_bridges(AddStandinMeshFn mesh, AttachTilesetFn tileset,
                              InvalidateFn invalidate) {
  add_mesh_ = std::move(mesh);
  attach_tileset_ = std::move(tileset);
  invalidate_ = std::move(invalidate);
}

void Scene3dSink::invalidate() const {
  if (invalidate_) {
    invalidate_();
  }
}

void Scene3dSink::set_overlay_bridges(SetOverlayTinMeshFn mesh,
                                      SetOverlayTinDrapeFn drape,
                                      ClearOverlayTinFn clear) {
  overlay_tin_mesh_fn_ = std::move(mesh);
  overlay_tin_drape_fn_ = std::move(drape);
  clear_overlay_tin_fn_ = std::move(clear);
}

void Scene3dSink::set_earth_bridges(OpenEarthFn open_earth, FlyToFn fly_to,
                                    LoadGlobalDemFn load_global_dem,
                                    SetSatelliteCloudFn set_satellite_cloud,
                                    SetAtmosphereFn set_atmosphere) {
  open_earth_ = std::move(open_earth);
  fly_to_ = std::move(fly_to);
  load_global_dem_ = std::move(load_global_dem);
  set_satellite_cloud_ = std::move(set_satellite_cloud);
  set_atmosphere_ = std::move(set_atmosphere);
  earth_bridges_installed_ = static_cast<bool>(open_earth_);
}

void Scene3dSink::set_look_bridges(ApplyLookFn apply_look,
                                   FlyGlobeFn fly_globe) {
  apply_look_ = std::move(apply_look);
  fly_globe_ = std::move(fly_globe);
  look_bridges_installed_ = static_cast<bool>(apply_look_);
}

bool Scene3dSink::set_overlay_tin_mesh(const float* xyz_lon_lat_elev,
                                       int point_count,
                                       const unsigned* indices, int index_count,
                                       const uint8_t* albedo_rgba) const {
  return overlay_tin_mesh_fn_
             ? overlay_tin_mesh_fn_(xyz_lon_lat_elev, point_count, indices,
                                    index_count, albedo_rgba)
             : false;
}

bool Scene3dSink::set_overlay_tin_drape(const uint8_t* rgba, uint32_t width,
                                        uint32_t height, const float* uv,
                                        int uv_float_count) const {
  return overlay_tin_drape_fn_
             ? overlay_tin_drape_fn_(rgba, width, height, uv, uv_float_count)
             : false;
}

void Scene3dSink::clear_overlay_tin_mesh() const {
  if (clear_overlay_tin_fn_) {
    clear_overlay_tin_fn_();
  }
}

}  // namespace plugin
