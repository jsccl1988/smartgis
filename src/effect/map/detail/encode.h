// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Command-list encode: clear, then draw uploaded meshes in order. No resource
// creation.

#ifndef EFFECT_MAP_DETAIL_ENCODE_H_
#define EFFECT_MAP_DETAIL_ENCODE_H_

#include <vector>

#include "gis/vista/frame/frame.h"
#include "effect/map/detail/upload.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace map {
namespace detail {

// Binds |camera| when set; otherwise the ortho of |view|. |load_op| kClear
// uses the frame background. |close_list| is false when a later pass still
// records on this list. |solid| and |textured| are programs the pass created
// on the same Device.
void encode_draws(render::rhi::CommandList* list, const gis::vista::View& view,
                  const gis::vista::MapFrame& frame,
                  const std::vector<UploadedDraw>& draws,
                  const render::rhi::CameraMatrices* camera,
                  render::rhi::ColorLoadOp load_op, bool close_list,
                  render::rhi::Pipeline* solid, render::rhi::Pipeline* textured);

}  // namespace detail
}  // namespace map
}  // namespace effect

#endif  // EFFECT_MAP_DETAIL_ENCODE_H_
