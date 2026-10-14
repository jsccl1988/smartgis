// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/hud/hud_pass.h"

#include "render/programs/programs.h"
#include "vista/pass/world/hud/hud_raster.h"

#include <cstring>
#include <vector>

namespace vista {
namespace {

void bgra_to_rgba(const uint8_t* bgra, uint32_t w, uint32_t h,
                  std::vector<uint8_t>* rgba) {
  rgba->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0);
  for (uint32_t i = 0; i < w * h; ++i) {
    (*rgba)[i * 4u + 0] = bgra[i * 4u + 2];
    (*rgba)[i * 4u + 1] = bgra[i * 4u + 1];
    (*rgba)[i * 4u + 2] = bgra[i * 4u + 0];
    (*rgba)[i * 4u + 3] = bgra[i * 4u + 3];
  }
}

}  // namespace

HudPass::~HudPass() {
  abandon_gpu();
}

void HudPass::bind(HudFrame frame) {
  frame_ = std::move(frame);
}

void HudPass::clear() {
  frame_.clear();
  bgra_.clear();
  raster_generation_ = 0;
}

void HudPass::abandon_gpu() {
  vb_ = nullptr;
  ib_ = nullptr;
  texture_ = nullptr;
  pipeline_ = nullptr;
  device_ = nullptr;
  uploaded_generation_ = 0;
  uploaded_w_ = 0;
  uploaded_h_ = 0;
}

void HudPass::release(render::rhi::Device* device) {
  if (device && device_ == device) {
    release_device_resources();
  } else {
    abandon_gpu();
  }
}

void HudPass::release_device_resources() {
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

bool HudPass::ensure_pipeline(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (device_ != device) {
    release_device_resources();
    device_ = device;
  }
  if (!pipeline_) {
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

bool HudPass::ensure_texture(render::rhi::Device* device, uint32_t width_px,
                             uint32_t height_px) {
  if (!device || frame_.empty() || width_px == 0 || height_px == 0) {
    return false;
  }
  if (frame_.width_px != width_px || frame_.height_px != height_px) {
    return false;
  }
  if (raster_generation_ != frame_.generation || bgra_.empty()) {
    if (!raster_hud_frame_bgra(frame_, &bgra_)) {
      return false;
    }
    raster_generation_ = frame_.generation;
  }
  if (texture_ && frame_.generation != 0 &&
      frame_.generation == uploaded_generation_ && uploaded_w_ == width_px &&
      uploaded_h_ == height_px) {
    return true;
  }
  if (texture_) {
    device->destroy_texture(texture_);
    texture_ = nullptr;
  }
  std::vector<uint8_t> rgba;
  bgra_to_rgba(bgra_.data(), width_px, height_px, &rgba);
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
  uploaded_generation_ = frame_.generation;
  uploaded_w_ = width_px;
  uploaded_h_ = height_px;
  return true;
}

bool HudPass::record(render::rhi::Device* device,
                     render::rhi::CommandList* list, uint32_t width_px,
                     uint32_t height_px) {
  if (frame_.empty()) {
    return true;
  }
  if (!device || !list || width_px == 0 || height_px == 0) {
    return true;
  }
  if (frame_.width_px != width_px || frame_.height_px != height_px) {
    return true;
  }
  if (!ensure_pipeline(device) ||
      !ensure_texture(device, width_px, height_px) || !texture_) {
    // Fail soft on HUD only — terrain already recorded. Product SoT is GPU
    // mesh; missing chrome must not fail the frame.
    return true;
  }

  const float sw = static_cast<float>(width_px);
  const float sh = static_cast<float>(height_px);
  const float verts[20] = {
      0.f, sh, 0.f, 0.f, 0.f, sw, sh, 0.f, 1.f, 0.f,
      sw,  0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
  };
  if (vb_) {
    device->destroy_buffer(vb_);
    vb_ = nullptr;
  }
  vb_ = device->create_buffer(sizeof(verts), render::rhi::BufferUsage::kVertex);
  if (!vb_ || !device->upload(vb_, verts, sizeof(verts))) {
    device->destroy_buffer(vb_);
    vb_ = nullptr;
    return true;
  }

  render::rhi::RenderPassDesc desc;
  desc.width = width_px;
  desc.height = height_px;
  desc.load_op = render::rhi::ColorLoadOp::kLoad;
  list->begin_render_pass(desc);
  list->set_viewport(0.f, 0.f, sw, sh, 0.f, 1.f);
  list->set_depth_mode(render::rhi::DepthMode::kDisabled);
  list->bind_camera(
      render::rhi::make_ortho_camera(0.f, sw, 0.f, sh, -1.f, 1.f));
  list->bind_texture(texture_, 0);
  list->set_pipeline(pipeline_);
  list->set_blend_mode(render::rhi::BlendMode::kSrcAlpha);
  const render::programs::Color tint{1.f, 1.f, 1.f, 1.f};
  list->set_constants(render::programs::kColorSlot, &tint, sizeof(tint));
  list->bind_vertex_buffer(vb_, 0, 5u * sizeof(float));
  list->bind_index_buffer(ib_, 0);
  list->draw_indexed(6, 1, 0, 0, 0);
  list->end_render_pass();
  return true;
}

}  // namespace vista
