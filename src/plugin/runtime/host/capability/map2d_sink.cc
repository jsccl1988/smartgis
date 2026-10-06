// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/capability/map2d_sink.h"

namespace plugin {

void Map2dSink::set_bridges(AddStandinLayerFn layer, AttachDatasetFn dataset,
                            InvalidateFn invalidate) {
  add_layer_ = std::move(layer);
  attach_dataset_ = std::move(dataset);
  invalidate_ = std::move(invalidate);
}

void Map2dSink::invalidate() const {
  if (invalidate_) {
    invalidate_();
  }
}

void Map2dSink::set_view_bridges(OpenMapFn open_map, FrameToFn frame_to,
                                 LoadHillshadeFn load_hillshade) {
  open_map_ = std::move(open_map);
  frame_to_ = std::move(frame_to);
  load_hillshade_ = std::move(load_hillshade);
  view_bridges_installed_ = static_cast<bool>(open_map_);
}

void Map2dSink::set_look_bridges(ApplyLookFn apply_look, FrameFlyFn frame_fly) {
  apply_look_ = std::move(apply_look);
  frame_fly_ = std::move(frame_fly);
  look_bridges_installed_ = static_cast<bool>(apply_look_);
}

void Map2dSink::set_seed_bridges(SeedIfEmptyFn seed) {
  seed_if_empty_ = std::move(seed);
}

void Map2dSink::set_present_bridges(PresentGpuFn present_gpu,
                                    ExportBmpFn export_bmp) {
  present_gpu_ = std::move(present_gpu);
  export_bmp_ = std::move(export_bmp);
  present_bridges_installed_ = static_cast<bool>(export_bmp_);
}

}  // namespace plugin
