// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/fog/fog_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "effect/atmosphere/detail/math.h"
#include "effect/atmosphere/detail/mesh.h"
#include "effect/atmosphere/detail/raster.h"
#include "effect/atmosphere/fog/constants.h"
#include "effect/atmosphere/fog/hlsl.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace atmosphere {
namespace {

constexpr uint32_t kFogConstantSlot = 1;

render::rhi::GraphicsPipelineDesc fog_graphics_desc() {
  // Fullscreen NDC haze: pixel FogCB only (VS is clip pass-through).
  static constexpr render::rhi::BindingSlot kBindings[] = {
      {.slot = kFogConstantSlot,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = sizeof(FogConstants),
       .hlsl_name = "FogCB"},
  };
  render::rhi::GraphicsPipelineDesc desc;
  desc.vertex.hlsl = kVsFog;
  desc.pixel.hlsl = kPsFog;
  desc.vertex_layout = render::rhi::VertexLayout::kPosition;
  desc.bindings = kBindings;
  desc.binding_count = sizeof(kBindings) / sizeof(kBindings[0]);
  desc.blend = render::rhi::BlendMode::kSrcAlpha;
  desc.compile_depth_off = true;
  desc.compile_depth_write = false;
  desc.compile_depth_test = false;
  desc.camera_slot = -1;
  return desc;
}

}  // namespace

FogPass::FogPass() = default;

FogPass::~FogPass() {
  // FlyCube Device may already be shut down; abandon handles only.
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  release();
}

void FogPass::set_params(const FogDrawParams& params) {
  params_ = params;
  if (params_.density < 0.f) {
    params_.density = 0.f;
  }
  if (params_.visibility < 0.1f) {
    params_.visibility = 0.1f;
  }
  params_.max_opacity = detail::clampf(params_.max_opacity, 0.f, 1.f);
}

float FogPass::fog_factor(const FogDrawParams& p, float distance,
                         float height_y) {
  const float dist = std::max(0.0f, distance);
  const float vis = std::max(0.1f, p.visibility);
  const float dens = std::max(0.0f, p.density);
  // Exponential distance fog scaled by visibility length.
  const float dist_f = 1.0f - std::exp(-dens * (dist / vis));
  const float above = std::max(0.0f, height_y - p.base_height);
  const float height_f = std::exp(-above * std::max(0.0f, p.height_falloff));
  return detail::clampf(dist_f * height_f, 0.0f, 1.0f) *
         detail::clampf(p.max_opacity, 0.f, 1.f);
}

void FogPass::destroy_pipeline() {
  if (pipeline_ && pipeline_device_) {
    pipeline_device_->destroy_pipeline(pipeline_);
  }
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
}

bool FogPass::ensure_pipeline(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (pipeline_ && pipeline_device_ == device) {
    return true;
  }
  destroy_pipeline();
  pipeline_device_ = device;
  pipeline_ = device->create_graphics_pipeline(fog_graphics_desc());
  return pipeline_ != nullptr;
}

bool FogPass::ensure_fullscreen_mesh(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (detail::static_mesh_ready(device_, vertex_, index_, device)) {
    return true;
  }
  // Oversized NDC triangle covering the clip volume (same trick as many
  // fullscreen post passes). XY are clip coords; Z unused by the VS.
  const float ndc[9] = {
      -1.f, -1.f, 0.f, 3.f, -1.f, 0.f, -1.f, 3.f, 0.f,
  };
  const uint32_t indices[3] = {0, 1, 2};
  return detail::upload_static_mesh(
      &device_, &vertex_, &index_, nullptr, device, ndc,
      static_cast<uint32_t>(sizeof(ndc)), indices,
      static_cast<uint32_t>(sizeof(indices)));
}

bool FogPass::record(render::rhi::Device* device, render::rhi::CommandList* list, uint32_t width,
                     uint32_t height, const render::rhi::CameraMatrices* camera) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  if (!ensure_fullscreen_mesh(device)) {
    return false;
  }
  if (!ensure_pipeline(device)) {
    return false;
  }

  FogConstants fog{};
  fog.density = params_.density;
  fog.height_falloff = params_.height_falloff;
  fog.base_height = params_.base_height;
  fog.visibility = params_.visibility;
  fog.color_r = params_.color_r;
  fog.color_g = params_.color_g;
  fog.color_b = params_.color_b;
  fog.max_opacity = params_.max_opacity;
  if (camera) {
    detail::eye_from_view(camera->view, &fog.cam_x, &fog.cam_y, &fog.cam_z);
  }

  detail::begin_load_pass(list, width, height);
  detail::set_fullscreen_viewport(list, width, height);
  detail::apply_raster(
      list, {pipeline_, render::rhi::BlendMode::kSrcAlpha,
             render::rhi::DepthMode::kDisabled});
  list->set_constants(kFogConstantSlot, &fog,
                      static_cast<uint32_t>(sizeof(fog)));
  detail::draw_indexed_mesh(list, vertex_, index_, 3 * sizeof(float), 3);
  list->end_render_pass();
  return true;
}

void FogPass::release() {
  detail::release_static_mesh(&device_, &vertex_, &index_, nullptr);
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
}

}  // namespace atmosphere
}  // namespace effect
