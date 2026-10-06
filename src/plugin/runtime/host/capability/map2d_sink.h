// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_MAP2D_SINK_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_MAP2D_SINK_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// Shell-installed Map2d present facade. Chrome fills callbacks; product TUs
// never see Browser*. Registered on content::PluginHost as kCapabilityMap2d.
// 2D analogue of Scene3dSink (frame / present / seed / export). Not a globe
// atmosphere / ocean / TIN-drape copy.
class Map2dSink {
 public:
  using AddStandinLayerFn = std::function<bool(
      std::string_view name, double lon, double lat, double half_deg)>;
  using AttachDatasetFn = std::function<bool(std::string_view path)>;
  using InvalidateFn = std::function<void()>;
  using OpenMapFn = std::function<bool()>;
  using FrameToFn =
      std::function<bool(double lon, double lat, double span_deg)>;
  using LoadHillshadeFn =
      std::function<bool(std::string_view path, std::string* result_json)>;
  using ApplyLookFn =
      std::function<bool(std::string_view mode_id, std::string* result_json)>;
  using FrameFlyFn =
      std::function<bool(float t01, std::string* result_json)>;
  using SeedIfEmptyFn = std::function<void()>;
  using PresentGpuFn =
      std::function<bool(uint32_t width_px, uint32_t height_px)>;
  using ExportBmpFn = std::function<bool(std::string_view path, int width_px,
                                         int height_px)>;

  PLUGIN_HOST_EXPORT void set_bridges(AddStandinLayerFn layer,
                                      AttachDatasetFn dataset,
                                      InvalidateFn invalidate);
  PLUGIN_HOST_EXPORT void set_view_bridges(OpenMapFn open_map, FrameToFn frame_to,
                                           LoadHillshadeFn load_hillshade);
  PLUGIN_HOST_EXPORT void set_look_bridges(ApplyLookFn apply_look,
                                           FrameFlyFn frame_fly);
  PLUGIN_HOST_EXPORT void set_seed_bridges(SeedIfEmptyFn seed);
  PLUGIN_HOST_EXPORT void set_present_bridges(PresentGpuFn present_gpu,
                                              ExportBmpFn export_bmp);

  bool view_bridges_installed() const { return view_bridges_installed_; }
  bool look_bridges_installed() const { return look_bridges_installed_; }
  bool present_bridges_installed() const { return present_bridges_installed_; }

  bool add_standin_layer(std::string_view name, double lon, double lat,
                         double half_deg) const {
    return add_layer_ ? add_layer_(name, lon, lat, half_deg) : false;
  }
  bool attach_dataset(std::string_view path) const {
    return attach_dataset_ ? attach_dataset_(path) : false;
  }
  PLUGIN_HOST_EXPORT void invalidate() const;

  bool open_map() const { return open_map_ ? open_map_() : false; }
  bool frame_to(double lon, double lat, double span_deg) const {
    return frame_to_ ? frame_to_(lon, lat, span_deg) : false;
  }
  bool load_hillshade(std::string_view path, std::string* result_json) const {
    return load_hillshade_ ? load_hillshade_(path, result_json) : false;
  }
  bool apply_look(std::string_view mode_id, std::string* result_json) const {
    return apply_look_ ? apply_look_(mode_id, result_json) : false;
  }
  bool frame_fly(float t01, std::string* result_json) const {
    return frame_fly_ ? frame_fly_(t01, result_json) : false;
  }
  void seed_if_empty() const {
    if (seed_if_empty_) {
      seed_if_empty_();
    }
  }
  bool present_gpu(uint32_t width_px, uint32_t height_px) const {
    return present_gpu_ ? present_gpu_(width_px, height_px) : false;
  }
  bool export_bmp(std::string_view path, int width_px, int height_px) const {
    return export_bmp_ ? export_bmp_(path, width_px, height_px) : false;
  }

 private:
  AddStandinLayerFn add_layer_;
  AttachDatasetFn attach_dataset_;
  InvalidateFn invalidate_;
  OpenMapFn open_map_;
  FrameToFn frame_to_;
  LoadHillshadeFn load_hillshade_;
  ApplyLookFn apply_look_;
  FrameFlyFn frame_fly_;
  SeedIfEmptyFn seed_if_empty_;
  PresentGpuFn present_gpu_;
  ExportBmpFn export_bmp_;
  bool view_bridges_installed_ = false;
  bool look_bridges_installed_ = false;
  bool present_bridges_installed_ = false;
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_MAP2D_SINK_H_
