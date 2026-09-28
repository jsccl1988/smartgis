// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/frame/atmosphere_frame.h"

#include "effect/atmosphere/cloud/cloud_pass.h"
#include "effect/atmosphere/fog/fog_pass.h"
#include "effect/atmosphere/ocean/ocean_pass.h"
#include "effect/atmosphere/sky/sky_pass.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace atmosphere {

bool AtmosphereFrame::clears_color() const {
  return ocean_enabled_ || sky_enabled_;
}

bool AtmosphereFrame::uses_shared_depth() const {
  return ocean_enabled_ || cloud_enabled_ || sky_enabled_ || fog_enabled_;
}

bool AtmosphereFrame::record_pre_opaque(render::rhi::Device* device,
                                        render::rhi::CommandList* list, uint32_t width,
                                        uint32_t height,
                                        const render::rhi::CameraMatrices* camera) {
  if (!device || !list) {
    return false;
  }

  if (sky_enabled_ && !sky_pass_) {
    return false;
  }
  if (ocean_enabled_ && !ocean_pass_) {
    return false;
  }
  if (!sky_enabled_ && !ocean_enabled_) {
    return true;
  }

  // Sky tint drives the clear color when sky is on; else deep ocean blue.
  float clear_r = 0.05f;
  float clear_g = 0.12f;
  float clear_b = 0.22f;
  if (sky_enabled_ && sky_pass_) {
    SkyPass::average_sky_rgb(sky_pass_->params(), &clear_r, &clear_g, &clear_b);
  }

  render::rhi::RenderPassDesc pass;
  pass.width = width;
  pass.height = height;
  pass.clear_r = clear_r;
  pass.clear_g = clear_g;
  pass.clear_b = clear_b;
  pass.clear_a = 1.f;
  pass.load_op = render::rhi::ColorLoadOp::kClear;
  pass.enable_depth = true;
  pass.depth_load_op = render::rhi::DepthLoadOp::kClear;
  pass.depth_clear = 1.f;
  list->begin_render_pass(pass);

  bool ok = true;
  if (sky_enabled_) {
    ok = sky_pass_->record(device, list, width, height, camera) && ok;
  }
  if (ocean_enabled_) {
    ok = ocean_pass_->record(device, list, width, height, camera) && ok;
  }

  list->end_render_pass();
  return ok;
}

bool AtmosphereFrame::record_post_opaque(render::rhi::Device* device,
                                         render::rhi::CommandList* list, uint32_t width,
                                         uint32_t height,
                                         const render::rhi::CameraMatrices* camera,
                                         int cloud_quality) {
  if (!device || !list) {
    return false;
  }

  if (cloud_enabled_) {
    if (!cloud_pass_) {
      return false;
    }
    if (!cloud_pass_->record(device, list, width, height, camera,
                             cloud_quality)) {
      return false;
    }
  }

  if (fog_enabled_) {
    if (!fog_pass_) {
      return false;
    }
    if (!fog_pass_->record(device, list, width, height, camera)) {
      return false;
    }
  }

  return true;
}

}  // namespace atmosphere
}  // namespace effect
