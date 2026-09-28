// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/ocean/ocean_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "effect/atmosphere/common/field_texture.h"
#include "effect/atmosphere/detail/math.h"
#include "effect/atmosphere/detail/mesh.h"
#include "effect/atmosphere/detail/raster.h"
#include "effect/atmosphere/ocean/constants.h"
#include "effect/atmosphere/ocean/cpu_waves.h"
#include "effect/atmosphere/ocean/gpu_fields.h"
#include "effect/atmosphere/ocean/hlsl.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace atmosphere {
namespace {

constexpr uint32_t kOceanConstantSlot = 1;

render::rhi::GraphicsPipelineDesc ocean_graphics_desc() {
  static constexpr render::rhi::BindingSlot kBindings[] = {
      {.slot = 0,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kVertex,
       .size_bytes = 128,
       .hlsl_name = "CameraCB"},
      {.slot = kOceanConstantSlot,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kVertex,
       .size_bytes = sizeof(OceanConstants),
       .hlsl_name = "OceanCB"},
      {.slot = kOceanConstantSlot,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = sizeof(OceanConstants),
       .hlsl_name = "OceanCB"},
      {.slot = 0,
       .kind = render::rhi::BindingKind::kSrv,
       .stage = render::rhi::ShaderStage::kVertex,
       .size_bytes = 0,
       .hlsl_name = "height_map"},
      {.slot = 0,
       .kind = render::rhi::BindingKind::kSampler,
       .stage = render::rhi::ShaderStage::kVertex,
       .size_bytes = 0,
       .hlsl_name = "linear_sampler"},
  };
  render::rhi::GraphicsPipelineDesc desc;
  desc.vertex.hlsl = kVsOcean;
  desc.pixel.hlsl = kPsOcean;
  desc.vertex_layout = render::rhi::VertexLayout::kPositionUv;
  desc.bindings = kBindings;
  desc.binding_count = sizeof(kBindings) / sizeof(kBindings[0]);
  desc.blend = render::rhi::BlendMode::kOpaque;
  desc.compile_depth_off = false;
  desc.compile_depth_write = true;
  desc.compile_depth_test = false;
  desc.camera_slot = 0;
  return desc;
}

int next_pow2_clamped(int n, int lo, int hi) {
  int v = std::max(lo, std::min(hi, n));
  int p = 1;
  while (p < v) {
    p <<= 1;
  }
  return std::min(p, hi);
}

}  // namespace

OceanPass::OceanPass() : gpu_(new detail::OceanGpuFields) {}

OceanPass::~OceanPass() {
  // FlyCube Device may already be shut down; abandon handles only.
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  release();
  delete gpu_;
  gpu_ = nullptr;
}

void OceanPass::destroy_pipeline() {
  if (pipeline_ && pipeline_device_) {
    pipeline_device_->destroy_pipeline(pipeline_);
  }
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
}

bool OceanPass::ensure_pipeline(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (pipeline_ && pipeline_device_ == device) {
    return true;
  }
  destroy_pipeline();
  pipeline_device_ = device;
  pipeline_ = device->create_graphics_pipeline(ocean_graphics_desc());
  return pipeline_ != nullptr;
}

void OceanPass::set_params(const OceanDrawParams& params) {
  params_ = params;
  if (params_.mesh_resolution < 2) {
    params_.mesh_resolution = 2;
  }
  if (params_.fft_size > 0) {
    params_.fft_size = next_pow2_clamped(params_.fft_size, 16, 128);
  }
}

void OceanPass::set_sea_mask_texture(FieldTexture* mask) {
  sea_mask_tex_ = mask;
}

void OceanPass::set_sea_mask_cpu(int cols, int rows, const float* values,
                                 std::size_t value_count) {
  mask_cols_ = 0;
  mask_rows_ = 0;
  mask_cpu_.clear();
  if (!values || cols < 1 || rows < 1) {
    return;
  }
  const std::size_t need =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  if (value_count != need) {
    return;
  }
  mask_cols_ = cols;
  mask_rows_ = rows;
  mask_cpu_.assign(values, values + value_count);
}

float OceanPass::sample_sea_mask(float u, float v) const {
  if (!mask_cpu_.empty()) {
    return detail::sample_bilinear(mask_cpu_, mask_cols_, mask_rows_, u, v);
  }
  // No mask â†?treat as open sea.
  return 1.0f;
}

void OceanPass::rebuild_mesh_grid() {
  const int mesh_n = params_.mesh_resolution;
  vertex_count_ = static_cast<uint32_t>(mesh_n * mesh_n);
  positions_.assign(static_cast<std::size_t>(vertex_count_ * 5u), 0.0f);
  const float hx = params_.patch_half_x > 0.f ? params_.patch_half_x
                                              : params_.patch_half_extent;
  const float hz = params_.patch_half_z > 0.f ? params_.patch_half_z
                                              : params_.patch_half_extent;
  const float step_x = (hx * 2.0f) / static_cast<float>(mesh_n - 1);
  const float step_z = (hz * 2.0f) / static_cast<float>(mesh_n - 1);
  for (int jz = 0; jz < mesh_n; ++jz) {
    for (int ix = 0; ix < mesh_n; ++ix) {
      const std::size_t vi =
          static_cast<std::size_t>(jz * mesh_n + ix);
      // X increases westward (X=-lon). Field masks store west at u=0, so flip U.
      const float x =
          params_.patch_center_x - hx + static_cast<float>(ix) * step_x;
      const float z =
          params_.patch_center_z - hz + static_cast<float>(jz) * step_z;
      const float u =
          1.0f - static_cast<float>(ix) / static_cast<float>(mesh_n - 1);
      const float v = static_cast<float>(jz) / static_cast<float>(mesh_n - 1);
      positions_[vi * 5 + 0] = x;
      positions_[vi * 5 + 1] = params_.patch_y;
      positions_[vi * 5 + 2] = z;
      positions_[vi * 5 + 3] = u;
      positions_[vi * 5 + 4] = v;
    }
  }
}

void OceanPass::rebuild_displacement() {
  const int mesh_n = params_.mesh_resolution;
  rebuild_mesh_grid();
  heights_.assign(static_cast<std::size_t>(vertex_count_), 0.0f);
  disp_x_.assign(static_cast<std::size_t>(vertex_count_), 0.0f);
  disp_z_.assign(static_cast<std::size_t>(vertex_count_), 0.0f);

  const float hx = params_.patch_half_x > 0.f ? params_.patch_half_x
                                              : params_.patch_half_extent;
  const float hz = params_.patch_half_z > 0.f ? params_.patch_half_z
                                              : params_.patch_half_extent;
  const float half = (std::max)(hx, hz);

  std::vector<float> heights;
  std::vector<float> disp_x;
  std::vector<float> disp_z;

  const bool use_gerstner =
      params_.use_gerstner_fallback || params_.fft_size < 16;
  if (use_gerstner) {
    detail::build_gerstner_heights(mesh_n, params_.significant_wave_height,
                           params_.mean_direction_rad, params_.wind_speed,
                           time_sec_, half, &heights, &disp_x, &disp_z);
    float max_abs = 0.05f;
    float max_disp = 0.05f;
    for (std::size_t i = 0; i < heights.size(); ++i) {
      max_abs = std::max(max_abs, std::fabs(heights[i]));
      max_disp = std::max(max_disp, std::fabs(disp_x[i]));
      max_disp = std::max(max_disp, std::fabs(disp_z[i]));
    }
    height_scale_ = max_abs;
    disp_scale_ = max_disp;
  } else {
    const int n = params_.fft_size;
    std::vector<float> fft_h;
    std::vector<float> fft_dx;
    std::vector<float> fft_dz;
    detail::build_fft_fields(n, params_.significant_wave_height, params_.wind_speed,
                     params_.wind_direction_rad, time_sec_, params_.use_jonswap,
                     params_.jonswap_gamma, params_.chop, &fft_h, &fft_dx,
                     &fft_dz, &height_scale_, &disp_scale_);
    heights.assign(static_cast<std::size_t>(mesh_n * mesh_n), 0.0f);
    disp_x.assign(heights.size(), 0.0f);
    disp_z.assign(heights.size(), 0.0f);
    for (int jz = 0; jz < mesh_n; ++jz) {
      for (int ix = 0; ix < mesh_n; ++ix) {
        const float u = static_cast<float>(ix) / static_cast<float>(mesh_n - 1);
        const float v = static_cast<float>(jz) / static_cast<float>(mesh_n - 1);
        const float fx = u * static_cast<float>(n - 1);
        const float fy = v * static_cast<float>(n - 1);
        const int x0 = static_cast<int>(fx);
        const int y0 = static_cast<int>(fy);
        const int x1 = std::min(x0 + 1, n - 1);
        const int y1 = std::min(y0 + 1, n - 1);
        const float tx = fx - static_cast<float>(x0);
        const float ty = fy - static_cast<float>(y0);
        auto sample4 = [&](const std::vector<float>& g) {
          const float h00 = g[static_cast<std::size_t>(y0 * n + x0)];
          const float h10 = g[static_cast<std::size_t>(y0 * n + x1)];
          const float h01 = g[static_cast<std::size_t>(y1 * n + x0)];
          const float h11 = g[static_cast<std::size_t>(y1 * n + x1)];
          return (h00 * (1.0f - tx) + h10 * tx) * (1.0f - ty) +
                 (h01 * (1.0f - tx) + h11 * tx) * ty;
        };
        const std::size_t vi =
            static_cast<std::size_t>(jz * mesh_n + ix);
        heights[vi] = sample4(fft_h);
        disp_x[vi] = sample4(fft_dx);
        disp_z[vi] = sample4(fft_dz);
      }
    }
  }

  heights_ = heights;
  disp_x_ = disp_x;
  disp_z_ = disp_z;

  // Keep base grid planar; VS applies height + Dx/Dz from the height map.
}

void OceanPass::rebuild_indices_with_mask() {
  const int mesh_n = params_.mesh_resolution;
  indices_.clear();
  indices_.reserve(static_cast<std::size_t>((mesh_n - 1) * (mesh_n - 1) * 6));
  const float thr = params_.sea_mask_threshold;
  for (int jz = 0; jz < mesh_n - 1; ++jz) {
    for (int ix = 0; ix < mesh_n - 1; ++ix) {
      // Same U flip as rebuild_mesh_grid (X=-lon vs lon-increasing mask).
      const float u0 =
          1.0f - static_cast<float>(ix) / static_cast<float>(mesh_n - 1);
      const float v0 = static_cast<float>(jz) / static_cast<float>(mesh_n - 1);
      const float u1 =
          1.0f - static_cast<float>(ix + 1) / static_cast<float>(mesh_n - 1);
      const float v1 =
          static_cast<float>(jz + 1) / static_cast<float>(mesh_n - 1);
      const float m00 = sample_sea_mask(u0, v0);
      const float m10 = sample_sea_mask(u1, v0);
      const float m01 = sample_sea_mask(u0, v1);
      const float m11 = sample_sea_mask(u1, v1);
      // Discard quad if all corners are land.
      if (m00 < thr && m10 < thr && m01 < thr && m11 < thr) {
        continue;
      }
      const uint32_t i00 =
          static_cast<uint32_t>(jz * mesh_n + ix);
      const uint32_t i10 =
          static_cast<uint32_t>(jz * mesh_n + ix + 1);
      const uint32_t i01 =
          static_cast<uint32_t>((jz + 1) * mesh_n + ix);
      const uint32_t i11 =
          static_cast<uint32_t>((jz + 1) * mesh_n + ix + 1);
      indices_.push_back(i00);
      indices_.push_back(i10);
      indices_.push_back(i11);
      indices_.push_back(i00);
      indices_.push_back(i11);
      indices_.push_back(i01);
    }
  }
  index_count_ = static_cast<uint32_t>(indices_.size());
}

bool OceanPass::ensure_resources(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  const int mesh_n = params_.mesh_resolution;
  const uint32_t want_verts = static_cast<uint32_t>(mesh_n * mesh_n);
  const bool need_realloc =
      device_ != device || vertex_buffer_ == nullptr ||
      index_buffer_ == nullptr || cached_mesh_res_ != mesh_n ||
      vertex_count_ != want_verts;

  if (need_realloc) {
    detail::destroy_mesh_buffers(device_, &vertex_buffer_, &index_buffer_);
    gpu_->destroy_height(device_);
    device_ = device;
    cached_mesh_res_ = mesh_n;
    const uint32_t vb_bytes = want_verts * 5u * sizeof(float);
    const uint32_t max_idx =
        static_cast<uint32_t>((mesh_n - 1) * (mesh_n - 1) * 6);
    const uint32_t ib_bytes = max_idx * sizeof(uint32_t);
    vertex_buffer_ =
        device_->create_buffer(vb_bytes, render::rhi::BufferUsage::kVertex);
    index_buffer_ =
        device_->create_buffer(ib_bytes, render::rhi::BufferUsage::kIndex);
    if (!vertex_buffer_ || !index_buffer_) {
      return false;
    }
  }
  return true;
}

bool OceanPass::record(render::rhi::Device* device, render::rhi::CommandList* list,
                       uint32_t width, uint32_t height,
                       const render::rhi::CameraMatrices* camera) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  used_gpu_fft_ = false;

  const bool want_gpu =
      params_.prefer_gpu_fft && !params_.use_gerstner_fallback &&
      params_.fft_size >= 16 && device->supports_compute();

  if (want_gpu) {
    rebuild_mesh_grid();
    const float hs = std::max(0.05f, params_.significant_wave_height);
    height_scale_ = std::max(0.05f, hs * 0.55f);
    disp_scale_ =
        std::max(0.05f, hs * 0.45f * std::max(params_.chop, 0.0f));
  } else {
    rebuild_displacement();
  }

  rebuild_indices_with_mask();
  if (index_count_ == 0) {
    return true;
  }
  if (!ensure_resources(device)) {
    return false;
  }

  if (want_gpu) {
    if (!gpu_->record(device_, device, list, params_.fft_size, params_, time_sec_,
                      &height_scale_, &disp_scale_)) {
      // Compute unavailable at record time â€?fall back to CPU FFT.
      rebuild_displacement();
      if (!gpu_->upload_height(device_, device, params_.mesh_resolution, heights_,
                               disp_x_, disp_z_, height_scale_, disp_scale_)) {
        return false;
      }
    } else {
      used_gpu_fft_ = true;
    }
  } else {
    if (!gpu_->upload_height(device_, device, params_.mesh_resolution, heights_,
                             disp_x_, disp_z_, height_scale_, disp_scale_)) {
      return false;
    }
  }

  const uint32_t vb_bytes =
      static_cast<uint32_t>(positions_.size() * sizeof(float));
  const uint32_t ib_bytes =
      static_cast<uint32_t>(indices_.size() * sizeof(uint32_t));
  if (!device->upload(vertex_buffer_, positions_.data(), vb_bytes) ||
      !device->upload(index_buffer_, indices_.data(), ib_bytes)) {
    return false;
  }

  if (!ensure_pipeline(device)) {
    return false;
  }

  OceanConstants ocean{};
  ocean.deep[0] = params_.deep_r;
  ocean.deep[1] = params_.deep_g;
  ocean.deep[2] = params_.deep_b;
  ocean.deep[3] = params_.deep_a;
  ocean.shallow[0] = params_.shallow_r;
  ocean.shallow[1] = params_.shallow_g;
  ocean.shallow[2] = params_.shallow_b;
  ocean.shallow[3] = params_.shallow_a;
  ocean.fresnel_bias = params_.fresnel_bias;
  ocean.fresnel_power = params_.fresnel_power;
  ocean.height_scale = height_scale_;
  ocean.disp_scale = disp_scale_;
  if (camera) {
    // Orbit cameras put the eye translation in the view matrix translation.
    detail::eye_from_view(camera->view, &ocean.cam_x, &ocean.cam_y, &ocean.cam_z);
  }

  detail::set_fullscreen_viewport(list, width, height);
  detail::bind_camera_if(list, camera);
  detail::apply_raster(list, {pipeline_, render::rhi::BlendMode::kOpaque,
                              render::rhi::DepthMode::kWrite});
  list->set_constants(kOceanConstantSlot, &ocean,
                      static_cast<uint32_t>(sizeof(ocean)));
  list->bind_texture(gpu_->height(), 0);
  detail::draw_indexed_mesh(list, vertex_buffer_, index_buffer_,
                            5 * sizeof(float), index_count_);
  return true;
}

void OceanPass::release() {
  // Drop GPU pointers only. Scene3dRhiSession / MapViewport intentionally leak
  // the FlyCube Device facade after shutdown; destroy_buffer/texture on that
  // facade AVs (CEF set_document â†?bind_map â†?abandon_mesh crash).
  vertex_buffer_ = nullptr;
  index_buffer_ = nullptr;
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  if (gpu_) {
    gpu_->release();
  }
  device_ = nullptr;
  index_count_ = 0;
  vertex_count_ = 0;
  cached_mesh_res_ = 0;
  used_gpu_fft_ = false;
  positions_.clear();
  heights_.clear();
  disp_x_.clear();
  disp_z_.clear();
  indices_.clear();
  sea_mask_tex_ = nullptr;
  mask_cols_ = 0;
  mask_rows_ = 0;
  mask_cpu_.clear();
}

}  // namespace atmosphere
}  // namespace effect
