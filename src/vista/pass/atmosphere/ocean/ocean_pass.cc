// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/atmosphere/ocean/ocean_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "vista/pass/atmosphere/detail/field_texture.h"
#include "vista/component/atmosphere/detail/math.h"
#include "vista/pass/atmosphere/detail/mesh.h"
#include "vista/pass/atmosphere/detail/raster.h"
#include "vista/pass/atmosphere/ocean/constants.h"
#include "vista/component/atmosphere/ocean/cpu_waves.h"
#include "vista/pass/atmosphere/ocean/gpu_fields.h"
#include "vista/pass/atmosphere/ocean/hlsl.h"
#include "render/rhi/rhi.h"

namespace vista {
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
      {.slot = 0,
       .kind = render::rhi::BindingKind::kSrv,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = 0,
       .hlsl_name = "height_map"},
      {.slot = 0,
       .kind = render::rhi::BindingKind::kSampler,
       .stage = render::rhi::ShaderStage::kPixel,
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
  // Need kTestOnly so post-opaque ocean depth-tests against DEM without
  // rewriting depth (atmosphere.full land punch-through).
  desc.compile_depth_test = true;
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
  // Abandon only — see SkyPass::destroy_pipeline (dangling Device* AV).
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
  if (!pipeline_) {
    pipeline_device_ = nullptr;
    return false;
  }
  return true;
}

void OceanPass::set_params(const OceanDrawParams& params) {
  const bool topology_change =
      params.mesh_resolution != params_.mesh_resolution ||
      params.patch_center_x != params_.patch_center_x ||
      params.patch_center_z != params_.patch_center_z ||
      params.patch_y != params_.patch_y ||
      params.patch_half_x != params_.patch_half_x ||
      params.patch_half_z != params_.patch_half_z ||
      params.patch_half_extent != params_.patch_half_extent ||
      params.sea_mask_threshold != params_.sea_mask_threshold;
  params_ = params;
  if (params_.mesh_resolution < 2) {
    params_.mesh_resolution = 2;
  }
  if (params_.fft_size > 0) {
    params_.fft_size = next_pow2_clamped(params_.fft_size, 16, 128);
  }
  detail::normalize3(&params_.sun_x, &params_.sun_y, &params_.sun_z);
  if (params_.shininess < 1.0f) {
    params_.shininess = 1.0f;
  }
  if (topology_change) {
    topology_dirty_ = true;
  }
}

void OceanPass::set_sun_direction(float x, float y, float z) {
  params_.sun_x = x;
  params_.sun_y = y;
  params_.sun_z = z;
  detail::normalize3(&params_.sun_x, &params_.sun_y, &params_.sun_z);
}

void OceanPass::set_sun_from_azimuth_elevation(float azimuth_rad,
                                               float elevation_rad) {
  detail::sun_from_azimuth_elevation(azimuth_rad, elevation_rad, &params_.sun_x,
                                     &params_.sun_y, &params_.sun_z);
}

void OceanPass::set_sea_mask_texture(FieldTexture* mask) {
  sea_mask_tex_ = mask;
}

void OceanPass::set_sea_mask_cpu(int cols, int rows, const float* values,
                                 std::size_t value_count) {
  if (!values || cols < 1 || rows < 1) {
    if (mask_cols_ != 0 || mask_rows_ != 0 || !mask_cpu_.empty()) {
      mask_cols_ = 0;
      mask_rows_ = 0;
      mask_cpu_.clear();
      topology_dirty_ = true;
    }
    return;
  }
  const std::size_t need =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  if (value_count != need) {
    return;
  }
  if (mask_cols_ == cols && mask_rows_ == rows && mask_cpu_.size() == need &&
      std::memcmp(mask_cpu_.data(), values, need * sizeof(float)) == 0) {
    return;
  }
  mask_cols_ = cols;
  mask_rows_ = rows;
  mask_cpu_.assign(values, values + value_count);
  topology_dirty_ = true;
}

float OceanPass::sample_sea_mask(float u, float v) const {
  if (!mask_cpu_.empty()) {
    return detail::sample_bilinear(mask_cpu_, mask_cols_, mask_rows_, u, v);
  }
  // No mask ??treat as open sea.
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
  cached_mesh_res_ = mesh_n;
  cached_patch_cx_ = params_.patch_center_x;
  cached_patch_cz_ = params_.patch_center_z;
  cached_patch_y_ = params_.patch_y;
  cached_patch_hx_ = hx;
  cached_patch_hz_ = hz;
}

bool OceanPass::mesh_topology_matches_params() const {
  const float hx = params_.patch_half_x > 0.f ? params_.patch_half_x
                                              : params_.patch_half_extent;
  const float hz = params_.patch_half_z > 0.f ? params_.patch_half_z
                                              : params_.patch_half_extent;
  const uint32_t want_verts =
      static_cast<uint32_t>(params_.mesh_resolution * params_.mesh_resolution);
  // Do not key off cached_mesh_res_ - ensure_resources() also writes that for
  // GPU buffer sizing, which would falsely skip rebuild_mesh_grid().
  return !positions_.empty() && vertex_count_ == want_verts &&
         cached_patch_cx_ == params_.patch_center_x &&
         cached_patch_cz_ == params_.patch_center_z &&
         cached_patch_y_ == params_.patch_y &&
         cached_patch_hx_ == hx && cached_patch_hz_ == hz;
}

void OceanPass::rebuild_displacement() {
  const int mesh_n = params_.mesh_resolution;
  if (!mesh_topology_matches_params()) {
    rebuild_mesh_grid();
    topology_dirty_ = true;
  }

  const float hx = params_.patch_half_x > 0.f ? params_.patch_half_x
                                              : params_.patch_half_extent;
  const float hz = params_.patch_half_z > 0.f ? params_.patch_half_z
                                              : params_.patch_half_extent;
  const float half = (std::max)(hx, hz);

  const bool use_gerstner =
      params_.use_gerstner_fallback || params_.fft_size < 16;
  if (use_gerstner) {
    // Write straight into member buffers - avoid temp vectors + assign copy
    // on every present (interactive Scene3d hot path).
    detail::build_gerstner_heights(mesh_n, params_.significant_wave_height,
                           params_.mean_direction_rad, params_.wind_speed,
                           time_sec_, half, &heights_, &disp_x_, &disp_z_);
    float max_abs = 0.05f;
    float max_disp = 0.05f;
    for (std::size_t i = 0; i < heights_.size(); ++i) {
      max_abs = std::max(max_abs, std::fabs(heights_[i]));
      max_disp = std::max(max_disp, std::fabs(disp_x_[i]));
      max_disp = std::max(max_disp, std::fabs(disp_z_[i]));
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
    const std::size_t verts = static_cast<std::size_t>(mesh_n * mesh_n);
    heights_.assign(verts, 0.0f);
    disp_x_.assign(verts, 0.0f);
    disp_z_.assign(verts, 0.0f);
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
        heights_[vi] = sample4(fft_h);
        disp_x_[vi] = sample4(fft_dx);
        disp_z_[vi] = sample4(fft_dz);
      }
    }
  }

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
      // Require a sea majority. A single wet corner used to keep coastal
      // quads that still painted near-black ocean over mainland DEM texels.
      const int sea_corners = (m00 >= thr ? 1 : 0) + (m10 >= thr ? 1 : 0) +
                              (m01 >= thr ? 1 : 0) + (m11 >= thr ? 1 : 0);
      if (sea_corners < 3) {
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
    topology_dirty_ = true;  // fresh GPU buffers need VB/IB upload
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

bool OceanPass::prepare_gpu(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  used_gpu_fft_ = false;
  // Prefer CPU Gerstner/FFT fields for the height map upload; GPU FFT still
  // runs later inside record() when prefer_gpu_fft is set.
  rebuild_displacement();
  if (topology_dirty_ || indices_.empty() ||
      cached_mask_cols_ != mask_cols_ || cached_mask_rows_ != mask_rows_) {
    rebuild_indices_with_mask();
    cached_mask_cols_ = mask_cols_;
    cached_mask_rows_ = mask_rows_;
    topology_dirty_ = true;
  }
  if (index_count_ == 0) {
    height_prepared_ = false;
    return true;
  }
  if (!ensure_resources(device)) {
    return false;
  }
  if (!gpu_->upload_height(device_, device, params_.mesh_resolution, heights_,
                           disp_x_, disp_z_, height_scale_, disp_scale_)) {
    return false;
  }
  // Keep topology_dirty_ so record() still uploads VB/IB; only the height
  // map is ready for the DEM remesh window.
  height_prepared_ = true;
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

  // prepare_gpu() already ran Gerstner/FFT + height upload for this frame.
  // Do not rebuild_displacement again — that doubled ocean CPU on every present
  // (~5–9 ms Debug) while the staged height map was already current.
  if (!height_prepared_) {
    if (want_gpu) {
      if (!mesh_topology_matches_params() || topology_dirty_) {
        rebuild_mesh_grid();
        topology_dirty_ = true;
      }
      const float hs = std::max(0.05f, params_.significant_wave_height);
      height_scale_ = std::max(0.05f, hs * 0.55f);
      disp_scale_ =
          std::max(0.05f, hs * 0.45f * std::max(params_.chop, 0.0f));
    } else {
      rebuild_displacement();
    }
  }

  if (topology_dirty_ || indices_.empty() ||
      cached_mask_cols_ != mask_cols_ || cached_mask_rows_ != mask_rows_) {
    rebuild_indices_with_mask();
    cached_mask_cols_ = mask_cols_;
    cached_mask_rows_ = mask_rows_;
    topology_dirty_ = true;
  }
  if (index_count_ == 0) {
    height_prepared_ = false;
    return true;
  }
  // After prepare_gpu(), mesh/height GPU objects must stay stable - realloc
  // here would destroy height and FlyCube can recycle the live DEM albedo.
  if (!height_prepared_) {
    if (!ensure_resources(device)) {
      return false;
    }
    // Never run GPU FFT after DEM remesh (ensure_textures can steal DEM SRVs).
    // CPU height upload only when prepare_gpu did not already stage the map.
    if (want_gpu) {
      rebuild_displacement();
    }
    if (!gpu_->upload_height(device_, device, params_.mesh_resolution, heights_,
                             disp_x_, disp_z_, height_scale_, disp_scale_)) {
      return false;
    }
  } else if (!vertex_buffer_ || !index_buffer_ || !gpu_->height()) {
    height_prepared_ = false;
    return false;
  }
  height_prepared_ = false;

  if (topology_dirty_) {
    const uint32_t vb_bytes =
        static_cast<uint32_t>(positions_.size() * sizeof(float));
    const uint32_t ib_bytes =
        static_cast<uint32_t>(indices_.size() * sizeof(uint32_t));
    if (!device->upload(vertex_buffer_, positions_.data(), vb_bytes) ||
        !device->upload(index_buffer_, indices_.data(), ib_bytes)) {
      return false;
    }
    topology_dirty_ = false;
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
  ocean.sun_x = params_.sun_x;
  ocean.sun_y = params_.sun_y;
  ocean.sun_z = params_.sun_z;
  ocean.shininess = params_.shininess;
  if (camera) {
    // Orbit cameras put the eye translation in the view matrix translation.
    detail::eye_from_view(camera->view, &ocean.cam_x, &ocean.cam_y, &ocean.cam_z);
  }

  detail::set_fullscreen_viewport(list, width, height);
  detail::bind_camera_if(list, camera);
  // Test-only: DEM already filled depth in the opaque pass. Writing ocean depth
  // used to punch dark water through hypsometric land when sea-mask / z order
  // drifted (atmosphere.full near-black China).
  detail::apply_raster(list, {pipeline_, render::rhi::BlendMode::kOpaque,
                              render::rhi::DepthMode::kTestOnly});
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
  // facade AVs (CEF set_document ??bind_map ??abandon_mesh crash).
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
  height_prepared_ = false;
  positions_.clear();
  heights_.clear();
  disp_x_.clear();
  disp_z_.clear();
  indices_.clear();
  sea_mask_tex_ = nullptr;
  mask_cols_ = 0;
  mask_rows_ = 0;
  mask_cpu_.clear();
  topology_dirty_ = true;
  cached_patch_cx_ = 0.f;
  cached_patch_cz_ = 0.f;
  cached_patch_y_ = 0.f;
  cached_patch_hx_ = 0.f;
  cached_patch_hz_ = 0.f;
  cached_mask_cols_ = -1;
  cached_mask_rows_ = -1;
}

}  // namespace vista
