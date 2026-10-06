// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_SCENE3D_SINK_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_SCENE3D_SINK_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// Shell-installed Scene3D present facade. Chrome fills callbacks; product TUs
// never see Browser*. Registered on content::PluginHost as kCapabilityScene3d.
class Scene3dSink {
 public:
  using AddStandinMeshFn = std::function<bool(
      std::string_view name, double lon, double lat, double half_deg)>;
  using AttachTilesetFn = std::function<bool(std::string_view uri)>;
  using InvalidateFn = std::function<void()>;
  using OpenEarthFn = std::function<bool()>;
  using FlyToFn = std::function<bool(double lon, double lat, float distance,
                                     double span_deg)>;
  using LoadGlobalDemFn =
      std::function<bool(std::string_view path, std::string* result_json)>;
  using SetSatelliteCloudFn = std::function<bool(std::string_view path,
                                                 bool enabled,
                                                 std::string* result_json)>;
  using SetAtmosphereFn =
      std::function<bool(bool sky, bool ocean, bool cloud, bool fog)>;
  using ApplyLookFn =
      std::function<bool(std::string_view mode_id, std::string* result_json)>;
  using FlyGlobeFn =
      std::function<bool(float t01, std::string* result_json)>;
  using SetOverlayTinMeshFn = std::function<bool(
      const float* xyz_lon_lat_elev, int point_count, const unsigned* indices,
      int index_count, const uint8_t* albedo_rgba)>;
  using SetOverlayTinDrapeFn = std::function<bool(
      const uint8_t* rgba, uint32_t width, uint32_t height, const float* uv,
      int uv_float_count)>;
  using ClearOverlayTinFn = std::function<void()>;

  PLUGIN_HOST_EXPORT void set_bridges(AddStandinMeshFn mesh,
                                      AttachTilesetFn tileset,
                                      InvalidateFn invalidate);
  PLUGIN_HOST_EXPORT void set_overlay_bridges(SetOverlayTinMeshFn mesh,
                                              SetOverlayTinDrapeFn drape,
                                              ClearOverlayTinFn clear);
  PLUGIN_HOST_EXPORT void set_earth_bridges(
      OpenEarthFn open_earth, FlyToFn fly_to, LoadGlobalDemFn load_global_dem,
      SetSatelliteCloudFn set_satellite_cloud,
      SetAtmosphereFn set_atmosphere);
  PLUGIN_HOST_EXPORT void set_look_bridges(ApplyLookFn apply_look,
                                           FlyGlobeFn fly_globe);

  bool earth_bridges_installed() const { return earth_bridges_installed_; }
  bool look_bridges_installed() const { return look_bridges_installed_; }

  bool add_standin_mesh(std::string_view name, double lon, double lat,
                        double half_deg) const {
    return add_mesh_ ? add_mesh_(name, lon, lat, half_deg) : false;
  }
  bool attach_tileset(std::string_view uri) const {
    return attach_tileset_ ? attach_tileset_(uri) : false;
  }
  PLUGIN_HOST_EXPORT void invalidate() const;

  bool open_earth() const {
    return open_earth_ ? open_earth_() : false;
  }
  bool fly_to(double lon, double lat, float distance,
              double span_deg) const {
    return fly_to_ ? fly_to_(lon, lat, distance, span_deg) : false;
  }
  bool load_global_dem(std::string_view path,
                       std::string* result_json) const {
    return load_global_dem_ ? load_global_dem_(path, result_json) : false;
  }
  bool set_satellite_cloud(std::string_view path, bool enabled,
                           std::string* result_json) const {
    return set_satellite_cloud_
               ? set_satellite_cloud_(path, enabled, result_json)
               : false;
  }
  bool set_atmosphere(bool sky, bool ocean, bool cloud, bool fog) const {
    return set_atmosphere_ ? set_atmosphere_(sky, ocean, cloud, fog) : false;
  }
  bool apply_look(std::string_view mode_id, std::string* result_json) const {
    return apply_look_ ? apply_look_(mode_id, result_json) : false;
  }
  bool fly_globe(float t01, std::string* result_json) const {
    return fly_globe_ ? fly_globe_(t01, result_json) : false;
  }

  PLUGIN_HOST_EXPORT bool set_overlay_tin_mesh(
      const float* xyz_lon_lat_elev, int point_count, const unsigned* indices,
      int index_count, const uint8_t* albedo_rgba) const;
  PLUGIN_HOST_EXPORT bool set_overlay_tin_drape(const uint8_t* rgba,
                                                uint32_t width, uint32_t height,
                                                const float* uv,
                                                int uv_float_count) const;
  PLUGIN_HOST_EXPORT void clear_overlay_tin_mesh() const;

 private:
  AddStandinMeshFn add_mesh_;
  AttachTilesetFn attach_tileset_;
  InvalidateFn invalidate_;
  OpenEarthFn open_earth_;
  FlyToFn fly_to_;
  LoadGlobalDemFn load_global_dem_;
  SetSatelliteCloudFn set_satellite_cloud_;
  SetAtmosphereFn set_atmosphere_;
  ApplyLookFn apply_look_;
  FlyGlobeFn fly_globe_;
  SetOverlayTinMeshFn overlay_tin_mesh_fn_;
  SetOverlayTinDrapeFn overlay_tin_drape_fn_;
  ClearOverlayTinFn clear_overlay_tin_fn_;
  bool earth_bridges_installed_ = false;
  bool look_bridges_installed_ = false;
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_SCENE3D_SINK_H_
