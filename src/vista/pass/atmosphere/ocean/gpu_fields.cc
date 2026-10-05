// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/atmosphere/ocean/gpu_fields.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "vista/component/atmosphere/detail/math.h"
#include "vista/pass/atmosphere/ocean/constants.h"
#include "vista/component/atmosphere/ocean/cpu_waves.h"
#include "vista/pass/atmosphere/ocean/hlsl.h"
#include "vista/pass/atmosphere/ocean/ocean_pass.h"
#include "render/rhi/rhi.h"

namespace vista {
namespace detail {
namespace {

constexpr uint32_t kFftConstantSlot = 0;

constexpr render::rhi::BindingSlot kFftCb{
    .slot = kFftConstantSlot,
    .kind = render::rhi::BindingKind::kConstantBuffer,
    .stage = render::rhi::ShaderStage::kCompute,
    .size_bytes = sizeof(OceanFftConstants),
    .hlsl_name = "OceanFftCB",
};

constexpr render::rhi::BindingSlot kSpectrumBindings[] = {
    kFftCb,
    {.slot = 0,
     .kind = render::rhi::BindingKind::kUav,
     .stage = render::rhi::ShaderStage::kCompute,
     .size_bytes = 0,
     .hlsl_name = "spectrum_uav"},
    {.slot = 1,
     .kind = render::rhi::BindingKind::kUav,
     .stage = render::rhi::ShaderStage::kCompute,
     .size_bytes = 0,
     .hlsl_name = "spectrum_seed_uav"},
};

constexpr render::rhi::BindingSlot kFftIoBindings[] = {
    kFftCb,
    {.slot = 0,
     .kind = render::rhi::BindingKind::kSrv,
     .stage = render::rhi::ShaderStage::kCompute,
     .size_bytes = 0,
     .hlsl_name = "src_tex"},
    {.slot = 0,
     .kind = render::rhi::BindingKind::kUav,
     .stage = render::rhi::ShaderStage::kCompute,
     .size_bytes = 0,
     .hlsl_name = "dst_tex"},
};

constexpr render::rhi::BindingSlot kEncodeBindings[] = {
    kFftCb,
    {.slot = 0,
     .kind = render::rhi::BindingKind::kSrv,
     .stage = render::rhi::ShaderStage::kCompute,
     .size_bytes = 0,
     .hlsl_name = "src_tex"},
    {.slot = 0,
     .kind = render::rhi::BindingKind::kUav,
     .stage = render::rhi::ShaderStage::kCompute,
     .size_bytes = 0,
     .hlsl_name = "height_uav"},
};

// Shared by kCsOceanGaussianH / kCsOceanGaussianV (RGBA height ping-pong).
constexpr render::rhi::BindingSlot kGaussianBindings[] = {
    kFftCb,
    {.slot = 0,
     .kind = render::rhi::BindingKind::kSrv,
     .stage = render::rhi::ShaderStage::kCompute,
     .size_bytes = 0,
     .hlsl_name = "src_tex"},
    {.slot = 0,
     .kind = render::rhi::BindingKind::kUav,
     .stage = render::rhi::ShaderStage::kCompute,
     .size_bytes = 0,
     .hlsl_name = "dst_tex"},
};

render::rhi::ComputePipelineDesc compute_desc(
    const char* hlsl, const render::rhi::BindingSlot* bindings,
    uint32_t binding_count) {
  render::rhi::ComputePipelineDesc desc;
  desc.compute.hlsl = hlsl;
  desc.bindings = bindings;
  desc.binding_count = binding_count;
  return desc;
}

void write_fft(render::rhi::CommandList* list, const OceanFftConstants& cb) {
  list->set_constants(kFftConstantSlot, &cb, static_cast<uint32_t>(sizeof(cb)));
}

}  // namespace

OceanGpuFields::~OceanGpuFields() {
  // Owner OceanPass may already have abandoned a shut-down FlyCube Device.
  spectrum_ = nullptr;
  bit_reverse_ = nullptr;
  butterfly_ = nullptr;
  displace_ = nullptr;
  encode_ = nullptr;
  gaussian_h_ = nullptr;
  gaussian_v_ = nullptr;
  pipeline_device_ = nullptr;
}

void OceanGpuFields::destroy_pipelines() {
  // Abandon only — never virtual-call through a possibly recycled Device*
  // (same FlyCube shutdown policy as SkyPass / OceanPass::release).
  spectrum_ = nullptr;
  bit_reverse_ = nullptr;
  butterfly_ = nullptr;
  displace_ = nullptr;
  encode_ = nullptr;
  gaussian_h_ = nullptr;
  gaussian_v_ = nullptr;
  pipeline_device_ = nullptr;
}

bool OceanGpuFields::ensure_pipelines(render::rhi::Device* device) {
  if (pipeline_device_ == device && spectrum_ && bit_reverse_ && butterfly_ &&
      displace_ && encode_ && gaussian_h_ && gaussian_v_) {
    return true;
  }
  destroy_pipelines();
  if (!device) {
    return false;
  }
  const auto count_of = [](const auto& bindings) {
    return static_cast<uint32_t>(sizeof(bindings) / sizeof(bindings[0]));
  };
  spectrum_ = device->create_compute_pipeline(
      compute_desc(kCsOceanSpectrum, kSpectrumBindings,
                   count_of(kSpectrumBindings)));
  bit_reverse_ = device->create_compute_pipeline(
      compute_desc(kCsOceanBitReverse, kFftIoBindings, count_of(kFftIoBindings)));
  butterfly_ = device->create_compute_pipeline(
      compute_desc(kCsOceanButterfly, kFftIoBindings, count_of(kFftIoBindings)));
  displace_ = device->create_compute_pipeline(compute_desc(
      kCsOceanDisplacementSpectrum, kFftIoBindings, count_of(kFftIoBindings)));
  encode_ = device->create_compute_pipeline(
      compute_desc(kCsOceanHeightEncode, kEncodeBindings, count_of(kEncodeBindings)));
  gaussian_h_ = device->create_compute_pipeline(compute_desc(
      kCsOceanGaussianH, kGaussianBindings, count_of(kGaussianBindings)));
  gaussian_v_ = device->create_compute_pipeline(compute_desc(
      kCsOceanGaussianV, kGaussianBindings, count_of(kGaussianBindings)));
  if (!spectrum_ || !bit_reverse_ || !butterfly_ || !displace_ || !encode_ ||
      !gaussian_h_ || !gaussian_v_) {
    pipeline_device_ = device;
    destroy_pipelines();
    return false;
  }
  pipeline_device_ = device;
  return true;
}

void OceanGpuFields::destroy_height(render::rhi::Device* owner_device) {
  if (owner_device && height_) {
    owner_device->destroy_texture(height_);
    height_ = nullptr;
  }
  if (owner_device && blur_scratch_) {
    owner_device->destroy_texture(blur_scratch_);
    blur_scratch_ = nullptr;
  }
  height_n_ = 0;
}

void OceanGpuFields::release() {
  height_ = nullptr;
  blur_scratch_ = nullptr;
  spectrum_a_ = nullptr;
  spectrum_b_ = nullptr;
  spectrum_seed_ = nullptr;
  spectrum_ = nullptr;
  bit_reverse_ = nullptr;
  butterfly_ = nullptr;
  displace_ = nullptr;
  encode_ = nullptr;
  gaussian_h_ = nullptr;
  gaussian_v_ = nullptr;
  pipeline_device_ = nullptr;
  height_n_ = 0;
  spectrum_n_ = 0;
}

bool OceanGpuFields::ensure_textures(render::rhi::Device* owner_device,
                                     render::rhi::Device* device, int n) {
  if (!device || n < 16) {
    return false;
  }
  const bool need_spectrum = !spectrum_a_ || !spectrum_b_ || !spectrum_seed_ ||
                             spectrum_n_ != n || owner_device != device;
  if (need_spectrum) {
    if (spectrum_a_ && owner_device) {
      owner_device->destroy_texture(spectrum_a_);
      spectrum_a_ = nullptr;
    }
    if (spectrum_b_ && owner_device) {
      owner_device->destroy_texture(spectrum_b_);
      spectrum_b_ = nullptr;
    }
    if (spectrum_seed_ && owner_device) {
      owner_device->destroy_texture(spectrum_seed_);
      spectrum_seed_ = nullptr;
    }
    render::rhi::TextureDesc spec;
    spec.width = static_cast<uint32_t>(n);
    spec.height = static_cast<uint32_t>(n);
    spec.format = render::rhi::TextureFormat::kRg32Float;
    spec.usage = render::rhi::TextureUsage::kSampled | render::rhi::TextureUsage::kStorage;
    spectrum_a_ = device->create_texture(spec);
    spectrum_b_ = device->create_texture(spec);
    spectrum_seed_ = device->create_texture(spec);
    spectrum_n_ = n;
    if (!spectrum_a_ || !spectrum_b_ || !spectrum_seed_) {
      return false;
    }
  }

  const bool need_height =
      !height_ || !blur_scratch_ || height_n_ != n || owner_device != device;
  if (need_height) {
    if (height_ && owner_device) {
      owner_device->destroy_texture(height_);
      height_ = nullptr;
    }
    if (blur_scratch_ && owner_device) {
      owner_device->destroy_texture(blur_scratch_);
      blur_scratch_ = nullptr;
    }
    render::rhi::TextureDesc desc;
    desc.width = static_cast<uint32_t>(n);
    desc.height = static_cast<uint32_t>(n);
    desc.format = render::rhi::TextureFormat::kRgba8;
    desc.usage = render::rhi::TextureUsage::kSampled | render::rhi::TextureUsage::kStorage |
                 render::rhi::TextureUsage::kCopyDest;
    height_ = device->create_texture(desc);
    blur_scratch_ = device->create_texture(desc);
    height_n_ = n;
    if (!height_ || !blur_scratch_) {
      return false;
    }
  }
  return true;
}

bool OceanGpuFields::record(render::rhi::Device* owner_device, render::rhi::Device* device,
                            render::rhi::CommandList* list, int n,
                            const OceanDrawParams& params, double time_sec,
                            float* height_scale, float* disp_scale) {
  if (!device || !list || !device->supports_compute()) {
    return false;
  }
  if (!ensure_textures(owner_device, device, n)) {
    return false;
  }
  if (!ensure_pipelines(device)) {
    return false;
  }

  uint32_t log2_n = 0;
  for (int v = n; v > 1; v >>= 1) {
    ++log2_n;
  }

  constexpr float kPatch = 100.0f;
  const float hs = std::max(0.05f, params.significant_wave_height);
  const float wind = std::max(1.0f, params.wind_speed);
  const float amp_scale = compute_energy_amp_scale(
      n, kPatch, hs, wind, params.wind_direction_rad, params.use_jonswap,
      params.jonswap_gamma);
  const float h_scale = std::max(0.05f, hs * 0.55f);
  const float d_scale =
      std::max(0.05f, hs * 0.45f * std::max(params.chop, 0.0f));
  if (height_scale) {
    *height_scale = h_scale;
  }
  if (disp_scale) {
    *disp_scale = d_scale;
  }

  OceanFftConstants cb{};
  cb.size = static_cast<uint32_t>(n);
  cb.log2_size = log2_n;
  cb.stage = 0;
  cb.direction = 0;
  cb.time_sec = static_cast<float>(time_sec);
  cb.wind_speed = wind;
  cb.wind_dir_rad = params.wind_direction_rad;
  cb.amp_scale = amp_scale;
  cb.patch_size = kPatch;
  cb.height_scale = h_scale;
  cb.disp_scale = d_scale;
  cb.chop = params.chop;
  cb.spectrum_model = params.use_jonswap
                          ? static_cast<uint32_t>(OceanSpectrumModel::kJonswap)
                          : static_cast<uint32_t>(OceanSpectrumModel::kPhillips);
  cb.encode_channel = 0;
  cb.gamma = params.jonswap_gamma;

  const uint32_t groups = (static_cast<uint32_t>(n) + 7u) / 8u;

  auto run_1d = [&](uint32_t direction, render::rhi::Texture* src_start) {
    render::rhi::Texture* src = src_start;
    render::rhi::Texture* dst = (src_start == spectrum_a_) ? spectrum_b_ : spectrum_a_;
    cb.direction = direction;
    cb.stage = 0;
    list->set_pipeline(bit_reverse_);
    write_fft(list, cb);
    list->bind_compute_srv(src, 0);
    list->bind_compute_uav(dst, 0);
    list->dispatch(groups, groups, 1);
    list->uav_barrier();
    std::swap(src, dst);

    list->set_pipeline(butterfly_);
    for (uint32_t stage = 0; stage < log2_n; ++stage) {
      cb.stage = stage;
      write_fft(list, cb);
      list->bind_compute_srv(src, 0);
      list->bind_compute_uav(dst, 0);
      list->dispatch(groups, groups, 1);
      list->uav_barrier();
      std::swap(src, dst);
    }
    return src;
  };

  auto encode_channel = [&](render::rhi::Texture* field, uint32_t channel) {
    cb.encode_channel = channel;
    list->set_pipeline(encode_);
    write_fft(list, cb);
    list->bind_compute_srv(field, 0);
    list->bind_compute_uav(height_, 0);
    list->dispatch(groups, groups, 1);
    list->uav_barrier();
  };

  list->set_pipeline(spectrum_);
  write_fft(list, cb);
  list->bind_compute_uav(spectrum_a_, 0);
  list->bind_compute_uav(spectrum_seed_, 1);
  list->dispatch(groups, groups, 1);
  list->uav_barrier();

  render::rhi::Texture* after_rows = run_1d(0, spectrum_a_);
  render::rhi::Texture* height_field = run_1d(1, after_rows);
  encode_channel(height_field, 0);

  cb.direction = 0;
  list->set_pipeline(displace_);
  write_fft(list, cb);
  list->bind_compute_srv(spectrum_seed_, 0);
  list->bind_compute_uav(spectrum_a_, 0);
  list->dispatch(groups, groups, 1);
  list->uav_barrier();
  after_rows = run_1d(0, spectrum_a_);
  render::rhi::Texture* dx_field = run_1d(1, after_rows);
  encode_channel(dx_field, 1);

  cb.direction = 1;
  list->set_pipeline(displace_);
  write_fft(list, cb);
  list->bind_compute_srv(spectrum_seed_, 0);
  list->bind_compute_uav(spectrum_a_, 0);
  list->dispatch(groups, groups, 1);
  list->uav_barrier();
  after_rows = run_1d(0, spectrum_a_);
  render::rhi::Texture* dz_field = run_1d(1, after_rows);
  encode_channel(dz_field, 2);

  // Separable 5-tap Gaussian on the packed RGBA height map (H then V).
  list->set_pipeline(gaussian_h_);
  write_fft(list, cb);
  list->bind_compute_srv(height_, 0);
  list->bind_compute_uav(blur_scratch_, 0);
  list->dispatch(groups, groups, 1);
  list->uav_barrier();

  list->set_pipeline(gaussian_v_);
  write_fft(list, cb);
  list->bind_compute_srv(blur_scratch_, 0);
  list->bind_compute_uav(height_, 0);
  list->dispatch(groups, groups, 1);
  list->uav_barrier();

  return true;
}

bool OceanGpuFields::upload_height(render::rhi::Device* owner_device, render::rhi::Device* device,
                                   int n, const std::vector<float>& heights,
                                   const std::vector<float>& disp_x,
                                   const std::vector<float>& disp_z,
                                   float height_scale, float disp_scale) {
  if (!device || heights.empty()) {
    return false;
  }
  if (n < 2) {
    return false;
  }
  if (!height_ || height_n_ != n || owner_device != device) {
    if (height_ && owner_device) {
      owner_device->destroy_texture(height_);
      height_ = nullptr;
    }
    render::rhi::TextureDesc desc;
    desc.width = static_cast<uint32_t>(n);
    desc.height = static_cast<uint32_t>(n);
    desc.format = render::rhi::TextureFormat::kRgba8;
    desc.usage = render::rhi::TextureUsage::kSampled | render::rhi::TextureUsage::kCopyDest;
    height_ = device->create_texture(desc);
    height_n_ = n;
    if (!height_) {
      return false;
    }
  }
  const float h_scale = std::max(height_scale, 1.0e-3f);
  const float d_scale = std::max(disp_scale, 1.0e-3f);
  const std::size_t nbytes = static_cast<std::size_t>(n * n * 4);
  // Function-local reuse: avoid n*n*4 heap churn without changing class layout.
  static thread_local std::vector<uint8_t> upload_rgba;
  upload_rgba.assign(nbytes, 0);
  for (int i = 0; i < n * n; ++i) {
    const float h = heights[static_cast<std::size_t>(i)];
    const float dx = disp_x.empty() ? 0.f : disp_x[static_cast<std::size_t>(i)];
    const float dz = disp_z.empty() ? 0.f : disp_z[static_cast<std::size_t>(i)];
    const float enc_h = clampf(0.5f + 0.5f * (h / h_scale), 0.0f, 1.0f);
    const float enc_x = clampf(0.5f + 0.5f * (dx / d_scale), 0.0f, 1.0f);
    const float enc_z = clampf(0.5f + 0.5f * (dz / d_scale), 0.0f, 1.0f);
    upload_rgba[static_cast<std::size_t>(i * 4 + 0)] =
        static_cast<uint8_t>(enc_h * 255.0f + 0.5f);
    upload_rgba[static_cast<std::size_t>(i * 4 + 1)] =
        static_cast<uint8_t>(enc_x * 255.0f + 0.5f);
    upload_rgba[static_cast<std::size_t>(i * 4 + 2)] =
        static_cast<uint8_t>(enc_z * 255.0f + 0.5f);
    upload_rgba[static_cast<std::size_t>(i * 4 + 3)] = 255;
  }
  return device->upload_texture(height_, upload_rgba.data(),
                                static_cast<uint32_t>(upload_rgba.size()));
}

}  // namespace detail
}  // namespace vista
