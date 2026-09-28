// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_DETAIL_RASTER_H_
#define EFFECT_ATMOSPHERE_DETAIL_RASTER_H_

#include <cstdint>

#include "render/rhi/rhi.h"

namespace effect {
namespace atmosphere {
namespace detail {

// Pipeline handle + blend + depth bound together for one atmosphere draw.
// pipeline may be null; the command list then records a null program.
struct RasterState {
  render::rhi::Pipeline* pipeline = nullptr;
  render::rhi::BlendMode blend = render::rhi::BlendMode::kOpaque;
  render::rhi::DepthMode depth = render::rhi::DepthMode::kWrite;
};

inline void set_fullscreen_viewport(render::rhi::CommandList* list, uint32_t width,
                                    uint32_t height) {
  list->set_viewport(0.f, 0.f, static_cast<float>(width),
                     static_cast<float>(height), 0.f, 1.f);
}

inline void bind_camera_if(render::rhi::CommandList* list,
                           const render::rhi::CameraMatrices* camera) {
  if (camera) {
    list->bind_camera(*camera);
  }
}

inline void apply_raster(render::rhi::CommandList* list, RasterState state) {
  list->set_pipeline(state.pipeline);
  list->set_blend_mode(state.blend);
  list->set_depth_mode(state.depth);
}

// Color + depth load. Never clears. Used by post-opaque cloud and fog.
inline void begin_load_pass(render::rhi::CommandList* list, uint32_t width,
                            uint32_t height) {
  render::rhi::RenderPassDesc pass;
  pass.width = width;
  pass.height = height;
  pass.load_op = render::rhi::ColorLoadOp::kLoad;
  pass.enable_depth = true;
  pass.depth_load_op = render::rhi::DepthLoadOp::kLoad;
  list->begin_render_pass(pass);
}

}  // namespace detail
}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_DETAIL_RASTER_H_
