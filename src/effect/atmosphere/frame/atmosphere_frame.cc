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
  // Always clear: even with sky/ocean off, Scene3d needs a color+depth open
  // so opaque DEM uses depth_write. Color-only depth_off draws clear the
  // large interactive swapchain but rasterize nothing (640x480 showcase OK).
  return true;
}

bool AtmosphereFrame::uses_shared_depth() const {
  return true;
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

  // Hard daylight clear — never the sun/horizon average (that washed pink).
  float clear_r = 0.40f;
  float clear_g = 0.62f;
  float clear_b = 0.92f;
  if (sky_enabled_ && sky_pass_) {
    float zr = clear_r;
    float zg = clear_g;
    float zb = clear_b;
    SkyPass::sample_sky_rgb(sky_pass_->params(), 0.f, 1.f, 0.f, &zr, &zg, &zb);
    clear_r = zr;
    clear_g = zg;
    clear_b = zb;
  } else if (!sky_enabled_ && !ocean_enabled_) {
    // Match GpuScene default background when no sky dome.
    clear_r = 0.f;
    clear_g = 0.2f;
    clear_b = 0.4f;
  }

  bool ok = true;

  // Sky draws into a color-only pass so the depth_off PSO (no DS format)
  // matches the render pass. A depth-attached + depth_off draw is skipped by
  // D3D12 when the PSO depth format is UNDEFINED.
  if (sky_enabled_) {
    render::rhi::RenderPassDesc sky_pass;
    sky_pass.width = width;
    sky_pass.height = height;
    sky_pass.clear_r = clear_r;
    sky_pass.clear_g = clear_g;
    sky_pass.clear_b = clear_b;
    sky_pass.clear_a = 1.f;
    sky_pass.load_op = render::rhi::ColorLoadOp::kClear;
    sky_pass.enable_depth = false;
    list->begin_render_pass(sky_pass);
    ok = sky_pass_->record(device, list, width, height, camera) && ok;
    list->end_render_pass();
  }

  // Ocean (and a depth clear for later opaque) share depth with terrain.
  // Always open this pass — opaque DEM on large FlyCube HWNDs requires
  // depth_write; color-only depth_off leaves a blank clear.
  {
    render::rhi::RenderPassDesc depth_pass;
    depth_pass.width = width;
    depth_pass.height = height;
    depth_pass.load_op = sky_enabled_ ? render::rhi::ColorLoadOp::kLoad
                                      : render::rhi::ColorLoadOp::kClear;
    if (!sky_enabled_) {
      depth_pass.clear_r = clear_r;
      depth_pass.clear_g = clear_g;
      depth_pass.clear_b = clear_b;
      depth_pass.clear_a = 1.f;
    }
    depth_pass.enable_depth = true;
    depth_pass.depth_load_op = render::rhi::DepthLoadOp::kClear;
    depth_pass.depth_clear = 1.f;
    list->begin_render_pass(depth_pass);
    if (ocean_enabled_) {
      ok = ocean_pass_->record(device, list, width, height, camera) && ok;
    }
    list->end_render_pass();
  }

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
    render::rhi::Texture* depth = device->shared_depth_texture();
    if (!fog_pass_->record(device, list, width, height, camera, depth)) {
      return false;
    }
  }

  return true;
}

}  // namespace atmosphere
}  // namespace effect
