// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/fog/fog_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "effect/atmosphere/detail/math.h"
#include "effect/atmosphere/detail/mesh.h"
#include "effect/atmosphere/detail/raster.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace atmosphere {

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
  pipeline_ = device->create_graphics_pipeline(
      render::programs::solid_pipeline_desc());
  return pipeline_ != nullptr;
}

bool FogPass::ensure_fullscreen_mesh(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (detail::static_mesh_ready(device_, vertex_, index_, device)) {
    return true;
  }
  // Large camera-facing card in orbit space (covers the view frustum).
  const detail::XzQuad quad = detail::make_xz_quad(6.0f, 0.35f);
  return detail::upload_static_mesh(
      &device_, &vertex_, &index_, nullptr, device, quad.xyz,
      static_cast<uint32_t>(sizeof(quad.xyz)), quad.indices,
      static_cast<uint32_t>(sizeof(quad.indices)));
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

  float cam_dist = 4.0f;
  float cam_y = 0.5f;
  if (camera) {
    float eye_x = 0.f;
    float eye_y = 0.f;
    float eye_z = 0.f;
    detail::eye_from_view(camera->view, &eye_x, &eye_y, &eye_z);
    cam_dist = std::sqrt(eye_x * eye_x + eye_y * eye_y + eye_z * eye_z);
    cam_y = eye_y;
  }
  const float alpha = fog_factor(params_, cam_dist, cam_y);

  detail::begin_load_pass(list, width, height);
  detail::set_fullscreen_viewport(list, width, height);
  detail::bind_camera_if(list, camera);
  detail::apply_raster(
      list, {pipeline_, render::rhi::BlendMode::kSrcAlpha, render::rhi::DepthMode::kTestOnly});
  // Slot 1 is the solid program's color float4.
  const float color[4] = {params_.color_r, params_.color_g, params_.color_b,
                          alpha};
  list->set_constants(1, color, static_cast<uint32_t>(sizeof(color)));
  detail::draw_indexed_mesh(list, vertex_, index_, 3 * sizeof(float), 6);
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
