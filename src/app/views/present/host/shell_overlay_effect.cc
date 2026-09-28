// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/present/host/shell_overlay_effect.h"

#include "render/programs/programs.h"

#include <cstring>
#include <vector>

namespace app {
namespace detail {
namespace {

void bgra_to_rgba(const uint8_t* bgra, uint32_t stride_bytes, uint32_t w,
                  uint32_t h, std::vector<uint8_t>* rgba) {
  rgba->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0);
  for (uint32_t y = 0; y < h; ++y) {
    const uint8_t* src = bgra + static_cast<size_t>(y) * stride_bytes;
    uint8_t* dst =
        rgba->data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
    for (uint32_t x = 0; x < w; ++x) {
      dst[0] = src[2];
      dst[1] = src[1];
      dst[2] = src[0];
      dst[3] = src[3];
      src += 4;
      dst += 4;
    }
  }
}

}  // namespace

ShellOverlayEffect::ShellOverlayEffect() = default;

ShellOverlayEffect::~ShellOverlayEffect() {
  // MapViewport shuts the Device down (or the test deletes it) before the
  // presenter. destroy_* on that pointer reads freed FlyCube state (0xDD).
  // Live replacement still goes through release_device_resources() from
  // ensure_pipeline while the Device argument is in hand. FlyCube programs
  // stay in the device map until shutdown, so dropping handles does not leak
  // past the device.
  vb_ = nullptr;
  ib_ = nullptr;
  texture_ = nullptr;
  pipeline_ = nullptr;
  device_ = nullptr;
  uploaded_generation_ = 0;
  uploaded_w_ = 0;
  uploaded_h_ = 0;
}

void ShellOverlayEffect::bind(const ui::gfx::ShellRaster& shell,
                              uint64_t shell_generation) {
  shell_ = shell;
  shell_generation_ = shell_generation;
}

void ShellOverlayEffect::clear() {
  shell_ = {};
  shell_generation_ = 0;
}

render::graph::EffectSlot ShellOverlayEffect::slot() const {
  return render::graph::EffectSlot::kOverlay;
}

void ShellOverlayEffect::release_device_resources() {
  if (!device_) {
    return;
  }
  if (vb_) {
    device_->destroy_buffer(vb_);
    vb_ = nullptr;
  }
  if (ib_) {
    device_->destroy_buffer(ib_);
    ib_ = nullptr;
  }
  if (texture_) {
    device_->destroy_texture(texture_);
    texture_ = nullptr;
  }
  if (pipeline_) {
    device_->destroy_pipeline(pipeline_);
    pipeline_ = nullptr;
  }
  device_ = nullptr;
  uploaded_generation_ = 0;
  uploaded_w_ = 0;
  uploaded_h_ = 0;
}

bool ShellOverlayEffect::ensure_pipeline(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (device_ != device) {
    release_device_resources();
    device_ = device;
  }
  if (!pipeline_) {
    // FlyCube bakes blend into the PSO; set_blend_mode at draw time is a
    // no-op. HUD must src-over so alpha-0 map holes keep the map visible.
    render::rhi::GraphicsPipelineDesc desc =
        render::programs::textured_pipeline_desc();
    desc.blend = render::rhi::BlendMode::kSrcAlpha;
    pipeline_ = device->create_graphics_pipeline(desc);
  }
  if (!ib_) {
    const uint32_t indices[6] = {0, 1, 2, 0, 2, 3};
    ib_ = device->create_buffer(sizeof(indices),
                                render::rhi::BufferUsage::kIndex);
    if (!ib_ || !device->upload(ib_, indices, sizeof(indices))) {
      device->destroy_buffer(ib_);
      ib_ = nullptr;
      return false;
    }
  }
  return pipeline_ != nullptr && ib_ != nullptr;
}

bool ShellOverlayEffect::ensure_texture(render::rhi::Device* device,
                                        uint32_t width_px,
                                        uint32_t height_px) {
  if (!device || !shell_.bgra || width_px == 0 || height_px == 0) {
    return false;
  }
  if (shell_.width_px != width_px || shell_.height_px != height_px) {
    return false;
  }
  const uint32_t stride =
      shell_.stride_bytes != 0 ? shell_.stride_bytes : width_px * 4u;
  if (stride < width_px * 4u) {
    return false;
  }
  // Generation skip: reuse uploaded texture when HUD pixels are unchanged.
  if (texture_ && shell_generation_ != 0 &&
      shell_generation_ == uploaded_generation_ && uploaded_w_ == width_px &&
      uploaded_h_ == height_px) {
    return true;
  }
  if (texture_) {
    device->destroy_texture(texture_);
    texture_ = nullptr;
  }
  std::vector<uint8_t> rgba;
  bgra_to_rgba(shell_.bgra, stride, width_px, height_px, &rgba);
  render::rhi::TextureDesc desc;
  desc.width = width_px;
  desc.height = height_px;
  desc.format = render::rhi::TextureFormat::kRgba8;
  desc.usage = render::rhi::TextureUsage::kSampled |
               render::rhi::TextureUsage::kCopyDest;
  texture_ = device->create_texture(desc);
  if (!texture_ ||
      !device->upload_texture(texture_, rgba.data(),
                              static_cast<uint32_t>(rgba.size()))) {
    device->destroy_texture(texture_);
    texture_ = nullptr;
    return false;
  }
  uploaded_generation_ = shell_generation_;
  uploaded_w_ = width_px;
  uploaded_h_ = height_px;
  return true;
}

bool ShellOverlayEffect::record(const render::graph::RecordContext& ctx) {
  if (!shell_.bgra || shell_.width_px == 0 || shell_.height_px == 0) {
    return true;
  }
  if (!ctx.device || !ctx.list || ctx.width == 0 || ctx.height == 0) {
    return true;
  }
  // Stale shell after resize: skip rather than stretch or fail the frame.
  if (shell_.width_px != ctx.width || shell_.height_px != ctx.height) {
    return true;
  }
  if (!ensure_pipeline(ctx.device) ||
      !ensure_texture(ctx.device, ctx.width, ctx.height) || !texture_) {
    return true;
  }

  const float sw = static_cast<float>(ctx.width);
  const float sh = static_cast<float>(ctx.height);
  // Pixel space, y-down → ortho y-up (same convention as RhiComposer).
  const float verts[20] = {
      0.f, sh, 0.f, 0.f, 0.f, sw, sh, 0.f, 1.f, 0.f,
      sw,  0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
  };
  if (vb_) {
    ctx.device->destroy_buffer(vb_);
    vb_ = nullptr;
  }
  vb_ = ctx.device->create_buffer(sizeof(verts),
                                  render::rhi::BufferUsage::kVertex);
  if (!vb_ || !ctx.device->upload(vb_, verts, sizeof(verts))) {
    ctx.device->destroy_buffer(vb_);
    vb_ = nullptr;
    return true;
  }

  render::rhi::RenderPassDesc desc;
  desc.width = ctx.width;
  desc.height = ctx.height;
  // Always load: map / scene already recorded into this swapchain. Do not
  // trust ctx.color_op (MapEffect does not advertise clears_color).
  desc.load_op = render::rhi::ColorLoadOp::kLoad;
  ctx.list->begin_render_pass(desc);
  ctx.list->set_viewport(0.f, 0.f, sw, sh, 0.f, 1.f);
  ctx.list->set_depth_mode(render::rhi::DepthMode::kDisabled);
  ctx.list->bind_camera(
      render::rhi::make_ortho_camera(0.f, sw, 0.f, sh, -1.f, 1.f));
  ctx.list->bind_texture(texture_, 0);
  ctx.list->set_pipeline(pipeline_);
  // Stub records last_blend; FlyCube ignores this (PSO already kSrcAlpha).
  ctx.list->set_blend_mode(render::rhi::BlendMode::kSrcAlpha);
  const render::programs::Color tint{1.f, 1.f, 1.f, 1.f};
  ctx.list->set_constants(render::programs::kColorSlot, &tint, sizeof(tint));
  ctx.list->bind_vertex_buffer(vb_, 0, 5u * sizeof(float));
  ctx.list->bind_index_buffer(ib_, 0);
  ctx.list->draw_indexed(6, 1, 0, 0, 0);
  ctx.list->end_render_pass();
  return true;
}

}  // namespace detail
}  // namespace app
