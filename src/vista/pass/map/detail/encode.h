// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Command-list encode: clear, then draw uploaded meshes in order. No resource
// creation.

#ifndef VISTA_PASS_MAP_DETAIL_ENCODE_H_
#define VISTA_PASS_MAP_DETAIL_ENCODE_H_

#include <vector>

#include "vista/component/map/ir.h"
#include "vista/pass/map/detail/upload.h"
#include "render/rhi/rhi.h"

namespace vista {
namespace detail {

// Binds |camera| when set; otherwise the ortho of |view|. |load_op| kClear
// uses the frame background. |close_list| is false when a later pass still
// records on this list. |solid| and |textured| are programs the pass created
// on the same Device.
// |multiply| is the textured program compiled with BlendMode::kMultiply.
// kOver textured draws keep |textured| and kSrcAlpha.
void encode_draws(render::rhi::CommandList* list, const vista::View& view,
                  const vista::MapIR& frame,
                  const std::vector<UploadedDraw>& draws,
                  const render::rhi::CameraMatrices* camera,
                  render::rhi::ColorLoadOp load_op, bool close_list,
                  render::rhi::Pipeline* solid, render::rhi::Pipeline* textured,
                  render::rhi::Pipeline* multiply);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_PASS_MAP_DETAIL_ENCODE_H_
