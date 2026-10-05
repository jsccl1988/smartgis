// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/atmosphere/atmosphere_frame.h"

#include "vista/pass/atmosphere/cloud/cloud_pass.h"
#include "vista/pass/atmosphere/fog/fog_pass.h"
#include "vista/pass/atmosphere/globe/globe_pass.h"
#include "vista/pass/atmosphere/globe/sat_cloud_pass.h"
#include "vista/pass/atmosphere/ocean/ocean_pass.h"
#include "vista/pass/atmosphere/sky/sky_pass.h"
#include "render/rhi/rhi.h"

namespace vista {

bool AtmosphereFrame::clears_color() const {
  // Always clear: even with sky/ocean off, Scene3d needs a color+depth open
  // so opaque DEM uses depth_write. Color-only depth_off draws clear the
  // large interactive swapchain but rasterize nothing (640x480 showcase OK).
  return true;
}

bool AtmosphereFrame::uses_shared_depth() const {
  return true;
}

void AtmosphereFrame::set_clear_rgb(float r, float g, float b) {
  clear_r_ = r;
  clear_g_ = g;
  clear_b_ = b;
  clear_rgb_set_ = true;
}

void AtmosphereFrame::clear_clear_rgb() {
  clear_rgb_set_ = false;
  clear_r_ = 0.f;
  clear_g_ = 0.f;
  clear_b_ = 0.f;
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
  if (globe_enabled_ && !globe_pass_) {
    return false;
  }

  // Hard daylight clear - never the sun/horizon average (that washed pink).
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
  } else if (!sky_enabled_) {
    // Scene3dGpuPresent defers ocean to post-DEM, so pre often has ocean_off
    // even when Environment ocean is on. Explicit clear_rgb (legacy stereo)
    // stays black; otherwise use navy so bare FlyCube China (pitch~0.4 DEM
    // strip) still yields lit+diverse BMP samples — pure black clear made
    // plugin-showcase signal=0 despite textured draws.
    if (clear_rgb_set_) {
      clear_r = clear_r_;
      clear_g = clear_g_;
      clear_b = clear_b_;
    } else {
      clear_r = 0.f;
      clear_g = 0.2f;
      clear_b = 0.4f;
    }
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

  // Globe DEM, or a depth clear alone on the flat path. Flat ocean is drawn
  // in record_post_opaque after opaque DEM. Always open this pass — opaque
  // DEM on large FlyCube HWNDs requires depth_write.
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
    if (globe_enabled_) {
      if (!globe_pass_) {
        list->end_render_pass();
        return false;
      }
      ok = globe_pass_->record(device, list, width, height, camera) && ok;
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

  // Flat ocean after opaque DEM, before cloud / fog / sat. Globe near-earth
  // may overlay a tangent Gerstner patch on the East China Sea.
  if (ocean_enabled_) {
    if (!ocean_pass_) {
      return false;
    }
    render::rhi::RenderPassDesc ocean_pass;
    ocean_pass.width = width;
    ocean_pass.height = height;
    ocean_pass.load_op = render::rhi::ColorLoadOp::kLoad;
    ocean_pass.enable_depth = true;
    ocean_pass.depth_load_op = render::rhi::DepthLoadOp::kLoad;
    list->begin_render_pass(ocean_pass);
    if (camera) {
      list->bind_camera(*camera);
    }
    if (!ocean_pass_->record(device, list, width, height, camera)) {
      list->end_render_pass();
      return false;
    }
    list->end_render_pass();
  }

  // Globe path: satellite cloud shell over the DEM sphere.
  if (sat_cloud_enabled_) {
    if (!sat_cloud_pass_) {
      return false;
    }
    if (!sat_cloud_pass_->record(device, list, width, height, camera)) {
      return false;
    }
  }

  // Flat path volumetric clouds (skipped when globe sat-cloud owns weather).
  if (cloud_enabled_ && !globe_enabled_) {
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

}  // namespace vista
