// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/globe/sat_cloud_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "effect/atmosphere/detail/mesh.h"
#include "effect/atmosphere/detail/raster.h"
#include "effect/atmosphere/globe/constants.h"
#include "effect/atmosphere/globe/hlsl.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace atmosphere {
namespace {

constexpr uint32_t kSatCloudConstantSlot = 1;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

render::rhi::GraphicsPipelineDesc sat_cloud_graphics_desc() {
  static constexpr render::rhi::BindingSlot kBindings[] = {
      {.slot = 0,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kVertex,
       .size_bytes = 128,
       .hlsl_name = "CameraCB"},
      {.slot = kSatCloudConstantSlot,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = sizeof(SatCloudConstants),
       .hlsl_name = "SatCloudCB"},
      {.slot = 0,
       .kind = render::rhi::BindingKind::kSrv,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = 0,
       .hlsl_name = "cover_tex"},
      {.slot = 0,
       .kind = render::rhi::BindingKind::kSampler,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = 0,
       .hlsl_name = "linear_sampler"},
  };
  render::rhi::GraphicsPipelineDesc desc;
  desc.vertex.hlsl = kVsSatCloud;
  desc.pixel.hlsl = kPsSatCloud;
  desc.vertex_layout = render::rhi::VertexLayout::kPositionUv;
  desc.bindings = kBindings;
  desc.binding_count = sizeof(kBindings) / sizeof(kBindings[0]);
  desc.blend = render::rhi::BlendMode::kSrcAlpha;
  desc.compile_depth_off = false;
  desc.compile_depth_write = false;
  desc.compile_depth_test = true;
  desc.camera_slot = 0;
  return desc;
}

float hash21(float x, float y) {
  const float n =
      std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
  return n - std::floor(n);
}

}  // namespace

SatCloudPass::SatCloudPass() = default;

SatCloudPass::~SatCloudPass() {
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  release();
}

void SatCloudPass::set_params(const SatCloudDrawParams& params) {
  if (params_.shell_radius != params.shell_radius ||
      params_.lon_slices != params.lon_slices ||
      params_.lat_slices != params.lat_slices) {
    mesh_dirty_ = true;
  }
  params_ = params;
}

void SatCloudPass::set_cover_rgba(const uint8_t* rgba, int w, int h) {
  cover_.clear();
  cover_w_ = 0;
  cover_h_ = 0;
  if (!rgba || w <= 0 || h <= 0) {
    return;
  }
  const std::size_t n =
      static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
  cover_.assign(rgba, rgba + n);
  cover_w_ = w;
  cover_h_ = h;
  cover_dirty_ = true;
}

void SatCloudPass::seed_procedural_cover(int w, int h, uint32_t seed) {
  w = (std::max)(32, w);
  h = (std::max)(16, h);
  // Build in a local buffer then swap — avoids Debug STL orphan AVs when
  // GlobePass mesh rebuild follows immediately in the same present.
  std::vector<uint8_t> cover(
      static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u, 0);
  const float seed_f = static_cast<float>(seed) * 0.01f;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float u = static_cast<float>(x) / static_cast<float>(w);
      const float v = static_cast<float>(y) / static_cast<float>(h);
      // Mid-latitude storm belts + fine noise (stub sat cloud).
      const float belt =
          std::exp(-((v - 0.35f) * (v - 0.35f)) / 0.02f) * 0.55f +
          std::exp(-((v - 0.62f) * (v - 0.62f)) / 0.018f) * 0.45f;
      const float n0 = hash21(u * 18.f + seed_f, v * 12.f);
      const float n1 = hash21(u * 48.f - seed_f, v * 36.f);
      float cvr = belt * 0.65f + n0 * 0.25f + n1 * 0.15f;
      cvr = (std::max)(0.f, (std::min)(1.f, cvr));
      if (v < 0.08f || v > 0.92f) {
        cvr *= 0.15f;
      }
      const uint8_t c = static_cast<uint8_t>(cvr * 255.f);
      const std::size_t i =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
           static_cast<std::size_t>(x)) *
          4u;
      cover[i + 0] = c;
      cover[i + 1] = c;
      cover[i + 2] = c;
      cover[i + 3] = c;
    }
  }
  cover_.swap(cover);
  cover_w_ = w;
  cover_h_ = h;
  cover_dirty_ = true;
}

void SatCloudPass::rebuild_mesh() {
  const int nlon = (std::max)(8, params_.lon_slices);
  const int nlat = (std::max)(4, params_.lat_slices);
  const float R = params_.shell_radius;
  const std::size_t verts =
      static_cast<std::size_t>(nlon + 1) * static_cast<std::size_t>(nlat + 1);
  const std::size_t tris =
      static_cast<std::size_t>(nlon) * static_cast<std::size_t>(nlat) * 2u;
  std::vector<float> positions(verts * 5u, 0.f);
  std::vector<uint32_t> indices(tris * 3u, 0u);
  std::size_t wi = 0;
  for (int iy = 0; iy <= nlat; ++iy) {
    const float v = static_cast<float>(iy) / static_cast<float>(nlat);
    const float lat = kPi * (0.5f - v);
    const float cl = std::cos(lat);
    const float sl = std::sin(lat);
    for (int ix = 0; ix <= nlon; ++ix) {
      const float u = static_cast<float>(ix) / static_cast<float>(nlon);
      const float lon = kTwoPi * u - kPi;
      positions[wi++] = R * cl * std::sin(lon);
      positions[wi++] = R * sl;
      positions[wi++] = R * cl * std::cos(lon);
      positions[wi++] = u;
      positions[wi++] = v;
    }
  }
  std::size_t ii = 0;
  for (int iy = 0; iy < nlat; ++iy) {
    for (int ix = 0; ix < nlon; ++ix) {
      const uint32_t i0 =
          static_cast<uint32_t>(iy * (nlon + 1) + ix);
      const uint32_t i1 = i0 + 1;
      const uint32_t i2 = i0 + static_cast<uint32_t>(nlon + 1);
      const uint32_t i3 = i2 + 1;
      indices[ii++] = i0;
      indices[ii++] = i2;
      indices[ii++] = i1;
      indices[ii++] = i1;
      indices[ii++] = i2;
      indices[ii++] = i3;
    }
  }
  positions_.swap(positions);
  indices_.swap(indices);
  index_count_ = static_cast<uint32_t>(indices_.size());
  mesh_dirty_ = false;
}

void SatCloudPass::destroy_pipeline() {
  if (pipeline_ && pipeline_device_) {
    pipeline_device_->destroy_pipeline(pipeline_);
  }
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
}

bool SatCloudPass::ensure_pipeline(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (pipeline_ && pipeline_device_ == device) {
    return true;
  }
  destroy_pipeline();
  pipeline_device_ = device;
  pipeline_ = device->create_graphics_pipeline(sat_cloud_graphics_desc());
  return pipeline_ != nullptr;
}

bool SatCloudPass::ensure_gpu(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (cover_.empty()) {
    seed_procedural_cover();
  }
  if (mesh_dirty_ || positions_.empty()) {
    rebuild_mesh();
  }
  if (device_ != device) {
    vertex_buffer_ = nullptr;
    index_buffer_ = nullptr;
    cover_tex_ = nullptr;
    device_ = device;
  }
  const uint32_t vb_bytes =
      static_cast<uint32_t>(positions_.size() * sizeof(float));
  const uint32_t ib_bytes =
      static_cast<uint32_t>(indices_.size() * sizeof(uint32_t));
  if (!vertex_buffer_ || !index_buffer_) {
    if (!detail::upload_static_mesh(&device_, &vertex_buffer_, &index_buffer_,
                                    &index_count_, device, positions_.data(),
                                    vb_bytes, indices_.data(), ib_bytes)) {
      return false;
    }
  } else if (mesh_dirty_) {
    if (!device->upload(vertex_buffer_, positions_.data(), vb_bytes) ||
        !device->upload(index_buffer_, indices_.data(), ib_bytes)) {
      return false;
    }
  }
  if (!cover_tex_ || cover_dirty_) {
    if (!cover_tex_) {
      render::rhi::TextureDesc desc;
      desc.width = static_cast<uint32_t>(cover_w_);
      desc.height = static_cast<uint32_t>(cover_h_);
      desc.format = render::rhi::TextureFormat::kRgba8;
      desc.usage = render::rhi::TextureUsage::kSampled |
                   render::rhi::TextureUsage::kCopyDest;
      cover_tex_ = device->create_texture(desc);
    }
    if (!cover_tex_) {
      return false;
    }
    const uint32_t tex_bytes =
        static_cast<uint32_t>(cover_w_) * static_cast<uint32_t>(cover_h_) * 4u;
    if (!device->upload_texture(cover_tex_, cover_.data(), tex_bytes)) {
      return false;
    }
    cover_dirty_ = false;
  }
  return index_count_ > 0;
}

bool SatCloudPass::record(render::rhi::Device* device,
                          render::rhi::CommandList* list, uint32_t width,
                          uint32_t height,
                          const render::rhi::CameraMatrices* camera) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  if (!ensure_gpu(device) || !ensure_pipeline(device)) {
    return false;
  }

  SatCloudConstants cb{};
  cb.opacity = params_.opacity;
  cb.soft_edge = params_.soft_edge;
  cb.time_sec = static_cast<float>(time_sec_);

  detail::set_fullscreen_viewport(list, width, height);
  detail::bind_camera_if(list, camera);
  detail::begin_load_pass(list, width, height);
  detail::apply_raster(list, {pipeline_, render::rhi::BlendMode::kSrcAlpha,
                              render::rhi::DepthMode::kTestOnly});
  list->set_constants(kSatCloudConstantSlot, &cb,
                      static_cast<uint32_t>(sizeof(cb)));
  list->bind_texture(cover_tex_, 0);
  detail::draw_indexed_mesh(list, vertex_buffer_, index_buffer_,
                            5 * sizeof(float), index_count_);
  list->end_render_pass();
  return true;
}

void SatCloudPass::release() {
  vertex_buffer_ = nullptr;
  index_buffer_ = nullptr;
  cover_tex_ = nullptr;
  device_ = nullptr;
  index_count_ = 0;
}

}  // namespace atmosphere
}  // namespace effect
