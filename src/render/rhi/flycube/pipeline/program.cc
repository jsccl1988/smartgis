// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/flycube/pipeline/program.h"

#ifdef HAS_FLYCUBE
#include "render/rhi/flycube/pipeline/hlsl.h"
#include "base/core/log.h"

#include "BindingSet/BindingSet.h"
#include "CommandList/CommandList.h"
#include "Utilities/Common.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#endif

namespace render {
namespace rhi {
namespace detail {

#ifdef HAS_FLYCUBE

namespace {

uint64_t g_flycube_cb_epoch = 1;

}  // namespace

void bump_flycube_constant_epoch() { ++g_flycube_cb_epoch; }

uint64_t flycube_constant_epoch() { return g_flycube_cb_epoch; }

void FlycubeProgram::begin_frame_bindings() {
  frame_binding_sets_.clear();
}

namespace {

GpuTexture* find_texture(const std::vector<TextureBind>& binds, uint32_t slot) {
  for (const TextureBind& bind : binds) {
    if (bind.slot == slot) {
      return bind.texture;
    }
  }
  return nullptr;
}

::BlendDesc blend_desc(BlendMode mode) {
  // Value-initialize: FlyCube/D3D12 rejects PSOs when unset write-mask /
  // logic-op fields hold stack garbage (opaque `return {}` worked; SrcAlpha
  // / Multiply with a default-constructed BlendDesc did not).
  if (mode == BlendMode::kSrcAlpha) {
    ::BlendDesc alpha{};
    alpha.blend_enable = true;
    alpha.src_color_blend_factor = BlendFactor::kSrcAlpha;
    alpha.dst_color_blend_factor = BlendFactor::kOneMinusSrcAlpha;
    alpha.color_blend_op = BlendOp::kAdd;
    alpha.src_alpha_blend_factor = BlendFactor::kOne;
    alpha.dst_alpha_blend_factor = BlendFactor::kOneMinusSrcAlpha;
    alpha.alpha_blend_op = BlendOp::kAdd;
    return alpha;
  }
  if (mode == BlendMode::kMultiply) {
    // dst.rgb *= src.rgb. Leave dst alpha unchanged so src alpha is ignored.
    ::BlendDesc multiply{};
    multiply.blend_enable = true;
    multiply.src_color_blend_factor = BlendFactor::kZero;
    multiply.dst_color_blend_factor = BlendFactor::kSrcColor;
    multiply.color_blend_op = BlendOp::kAdd;
    multiply.src_alpha_blend_factor = BlendFactor::kZero;
    multiply.dst_alpha_blend_factor = BlendFactor::kOne;
    multiply.alpha_blend_op = BlendOp::kAdd;
    return multiply;
  }
  return {};
}

std::vector<InputLayoutDesc> input_layout(VertexLayout layout) {
  switch (layout) {
    case VertexLayout::kPositionUv:
      return {{0, "POSITION", gli::FORMAT_RGB32_SFLOAT_PACK32, 20, 0},
              {0, "TEXCOORD", gli::FORMAT_RG32_SFLOAT_PACK32, 20, 12}};
    case VertexLayout::kPositionNormal:
      return {{0, "POSITION", gli::FORMAT_RGB32_SFLOAT_PACK32, 24, 0},
              {0, "NORMAL", gli::FORMAT_RGB32_SFLOAT_PACK32, 24, 12}};
    case VertexLayout::kPositionNormalUv:
      // float3 pos + float3 nrm + float2 uv (32-byte stride).
      return {{0, "POSITION", gli::FORMAT_RGB32_SFLOAT_PACK32, 32, 0},
              {0, "NORMAL", gli::FORMAT_RGB32_SFLOAT_PACK32, 32, 12},
              {0, "TEXCOORD", gli::FORMAT_RG32_SFLOAT_PACK32, 32, 24}};
    case VertexLayout::kPosition:
    default:
      return {{0, "POSITION", gli::FORMAT_RGB32_SFLOAT_PACK32, 12, 0}};
  }
}

}  // namespace

bool FlycubeProgram::compile_shader(const ShaderSource& source, ::ShaderType type,
                                    const char* tag, std::shared_ptr<::Shader>* out) {
  if (!device_ || !out || !source.hlsl || source.hlsl[0] == '\0') {
    return false;
  }
  const char* entry =
      (source.entry && source.entry[0] != '\0') ? source.entry : "main";
  const char* profile =
      (source.profile && source.profile[0] != '\0') ? source.profile : "6_0";
  static uint32_t seq = 0;
  const std::string file =
      "smartgis_rhi_" + std::to_string(++seq) + "_" + (tag ? tag : "sh") + ".hlsl";
  std::string path;
  if (!write_temp_hlsl(file.c_str(), source.hlsl, &path)) {
    return false;
  }
  // FlyCube CompileShader aborts if dxcompiler.dll is missing beside the PE.
  if (!ensure_dxc_beside_exe()) {
    return false;
  }
  std::shared_ptr<::Shader> shader =
      device_->CompileShader({path, entry, type, profile});
  if (!shader || shader->GetBlob().empty()) {
    return false;
  }
  *out = std::move(shader);
  return true;
}

bool FlycubeProgram::attach_bindings(const BindingSlot* bindings,
                                    uint32_t count) {
  if (count > 0 && bindings == nullptr) {
    return false;
  }
  std::vector<BindKey> keys;
  keys.reserve(count);
  bool needs_sampler = false;
  try {
    for (uint32_t i = 0; i < count; ++i) {
      const BindingSlot& slot = bindings[i];
      std::shared_ptr<::Shader> shader = shader_for(slot.stage);
      if (!shader || !slot.hlsl_name || slot.hlsl_name[0] == '\0') {
        return false;
      }
      // One BindingSlot per stage. The same hlsl name on VS and PS (OceanCB)
      // becomes two BindKeys that share one upload buffer.
      SlotBinding bound;
      bound.kind = slot.kind;
      bound.slot = slot.slot;
      bound.size_bytes = slot.size_bytes;
      bound.key = shader->GetBindKey(slot.hlsl_name);
      keys.push_back(bound.key);

      if (slot.kind == BindingKind::kConstantBuffer) {
        if (slot.size_bytes == 0) {
          return false;
        }
        const UploadCb* existing = find_cb(slot.slot);
        if (existing == nullptr) {
          const uint64_t cb_align = device_->GetConstantBufferOffsetAlignment();
          const uint64_t align = cb_align ? cb_align : 256;
          UploadCb cb;
          cb.slot = slot.slot;
          cb.size_bytes = slot.size_bytes;
          cb.buffer = device_->CreateBuffer(
              MemoryType::kUpload,
              {.size = Align(slot.size_bytes, align),
               .usage = BindFlag::kConstantBuffer});
          if (!cb.buffer) {
            return false;
          }
          ViewDesc cb_view = {
              .view_type = ViewType::kConstantBuffer,
              .dimension = ViewDimension::kBuffer,
          };
          cb.view = device_->CreateView(cb.buffer, cb_view);
          if (!cb.view) {
            return false;
          }
          constants_.push_back(std::move(cb));
        }
      } else if (slot.kind == BindingKind::kSampler) {
        needs_sampler = true;
      }
      bindings_.push_back(bound);
    }
  } catch (const std::exception&) {
    return false;
  }

  if (needs_sampler) {
    sampler_ = device_->CreateSampler({
        .min_filter = SamplerFilter::kLinear,
        .mag_filter = SamplerFilter::kLinear,
        .mip_filter = SamplerFilter::kNearest,
        .address_mode_u = SamplerAddressMode::kClampToEdge,
        .address_mode_v = SamplerAddressMode::kClampToEdge,
        .address_mode_w = SamplerAddressMode::kClampToEdge,
    });
    ViewDesc sampler_view = {.view_type = ViewType::kSampler};
    sampler_view_ = device_->CreateView(sampler_, sampler_view);
    if (!sampler_view_) {
      return false;
    }
  }

  layout_ = device_->CreateBindingSetLayout({.bind_keys = keys});
  return layout_ != nullptr;
}

std::shared_ptr<::Pipeline> FlycubeProgram::make_graphics_pso(
    gli::format color, gli::format depth_format, bool use_depth,
    const ::DepthStencilDesc& depth, const ::BlendDesc& blend) const {
  ::GraphicsPipelineDesc pso;
  pso.shaders = {vs_, ps_};
  pso.layout = layout_;
  pso.input = input_;
  pso.color_formats = {color};
  pso.depth_stencil_format =
      use_depth ? depth_format : gli::format::FORMAT_UNDEFINED;
  pso.depth_stencil_desc = depth;
  pso.blend_desc = blend;
  return device_->CreateGraphicsPipeline(pso);
}

std::shared_ptr<::Shader> FlycubeProgram::shader_for(ShaderStage stage) const {
  switch (stage) {
    case ShaderStage::kVertex:
      return vs_;
    case ShaderStage::kPixel:
      return ps_;
    case ShaderStage::kCompute:
      return cs_;
  }
  return nullptr;
}

const FlycubeProgram::UploadCb* FlycubeProgram::find_cb(uint32_t slot) const {
  for (const UploadCb& cb : constants_) {
    if (cb.slot == slot) {
      return &cb;
    }
  }
  return nullptr;
}

FlycubeProgram::UploadCb* FlycubeProgram::find_cb_mut(uint32_t slot) {
  for (UploadCb& cb : constants_) {
    if (cb.slot == slot) {
      return &cb;
    }
  }
  return nullptr;
}

uint32_t FlycubeProgram::constant_slot_size(uint32_t slot) const {
  const UploadCb* cb = find_cb(slot);
  return cb ? cb->size_bytes : 0;
}

void FlycubeProgram::upload_slot(uint32_t slot, const void* data,
                                uint32_t byte_size) {
  (void)take_cb_view(slot, data, byte_size);
}

std::shared_ptr<::View> FlycubeProgram::take_cb_view(uint32_t slot,
                                                    const void* data,
                                                    uint32_t byte_size) {
  if (!data || byte_size == 0 || !device_) {
    return nullptr;
  }
  UploadCb* cb = find_cb_mut(slot);
  if (!cb || byte_size > cb->size_bytes) {
    return nullptr;
  }
  if (cb_epoch_seen_ != g_flycube_cb_epoch) {
    cb_epoch_seen_ = g_flycube_cb_epoch;
    for (UploadCb& slot_cb : constants_) {
      slot_cb.cursor = 0;
    }
  }
  if (cb->ring_buffers.empty() && cb->buffer && cb->view) {
    cb->ring_buffers.push_back(cb->buffer);
    cb->ring_views.push_back(cb->view);
  }
  if (cb->cursor >= cb->ring_buffers.size()) {
    const uint64_t cb_align = device_->GetConstantBufferOffsetAlignment();
    const uint64_t align = cb_align ? cb_align : 256;
    std::shared_ptr<::Resource> buffer = device_->CreateBuffer(
        MemoryType::kUpload,
        {.size = Align(cb->size_bytes, align),
         .usage = BindFlag::kConstantBuffer});
    if (!buffer) {
      return nullptr;
    }
    ViewDesc cb_view = {
        .view_type = ViewType::kConstantBuffer,
        .dimension = ViewDimension::kBuffer,
    };
    std::shared_ptr<::View> view = device_->CreateView(buffer, cb_view);
    if (!view) {
      return nullptr;
    }
    cb->ring_buffers.push_back(std::move(buffer));
    cb->ring_views.push_back(std::move(view));
  }
  const uint32_t index = cb->cursor++;
  cb->ring_buffers[index]->UpdateUploadBuffer(0, data, byte_size);
  return cb->ring_views[index];
}

std::shared_ptr<::Pipeline> FlycubeProgram::select_depth(bool pass_depth,
                                                        DepthMode mode) const {
  // Pass depth plus a write or test-only mode selects that variant.
  // Depth-attached + kDisabled needs depth_off_ds_ (matched DS format);
  // color-only passes keep depth_off_. A missing variant skips the draw.
  if (pass_depth && mode == DepthMode::kWrite) {
    return depth_write_;
  }
  if (pass_depth && mode == DepthMode::kTestOnly) {
    return depth_test_;
  }
  if (pass_depth && mode == DepthMode::kDisabled) {
    return depth_off_ds_ ? depth_off_ds_ : depth_off_;
  }
  return depth_off_;
}

std::shared_ptr<::BindingSet> FlycubeProgram::make_binding_set(
    const std::vector<TextureBind>& srvs,
    const std::vector<TextureBind>& uavs,
    const std::vector<std::pair<uint32_t, std::shared_ptr<::View>>>* cb_views)
    const {
  if (!device_ || !layout_) {
    return nullptr;
  }
  std::vector<BindingDesc> writes;
  writes.reserve(bindings_.size());
  for (const SlotBinding& binding : bindings_) {
    std::shared_ptr<::View> view;
    switch (binding.kind) {
      case BindingKind::kConstantBuffer: {
        if (cb_views) {
          for (const auto& override_view : *cb_views) {
            if (override_view.first == binding.slot && override_view.second) {
              view = override_view.second;
              break;
            }
          }
        }
        if (!view) {
          const UploadCb* cb = find_cb(binding.slot);
          if (!cb) {
            return nullptr;
          }
          view = cb->view;
        }
        break;
      }
      case BindingKind::kSrv: {
        GpuTexture* tex = find_texture(srvs, binding.slot);
        if (!tex || !tex->srv()) {
          return nullptr;
        }
        view = tex->srv();
        break;
      }
      case BindingKind::kUav: {
        GpuTexture* tex = find_texture(uavs, binding.slot);
        if (!tex || !tex->uav()) {
          return nullptr;
        }
        view = tex->uav();
        break;
      }
      case BindingKind::kSampler:
        view = sampler_view_;
        break;
    }
    if (!view) {
      return nullptr;
    }
    writes.push_back({binding.key, view});
  }
  std::shared_ptr<::BindingSet> set = device_->CreateBindingSet(layout_);
  if (!set) {
    return nullptr;
  }
  set->WriteBindings({.bindings = writes});
  return set;
}

std::unique_ptr<FlycubeProgram> FlycubeProgram::compile_graphics(
    const std::shared_ptr<::Device>& device, const GraphicsPipelineDesc& desc,
    gli::format color_format, gli::format depth_format) {
  if (!device) {
    return nullptr;
  }
  auto program = std::unique_ptr<FlycubeProgram>(new FlycubeProgram());
  program->device_ = device;
  program->camera_slot_ = desc.camera_slot;
  program->input_ = input_layout(desc.vertex_layout);
  if (!program->compile_shader(desc.vertex, ::ShaderType::kVertex, "vs",
                               &program->vs_) ||
      !program->compile_shader(desc.pixel, ::ShaderType::kPixel, "ps",
                               &program->ps_) ||
      !program->attach_bindings(desc.bindings, desc.binding_count)) {
    return nullptr;
  }

  const ::BlendDesc blend = blend_desc(desc.blend);
  ::DepthStencilDesc depth_off;
  depth_off.depth_test_enable = false;
  depth_off.depth_write_enable = false;
  ::DepthStencilDesc depth_write;
  depth_write.depth_test_enable = true;
  depth_write.depth_write_enable = true;
  depth_write.depth_func = ComparisonFunc::kLess;
  ::DepthStencilDesc depth_test;
  depth_test.depth_test_enable = true;
  depth_test.depth_write_enable = false;
  depth_test.depth_func = ComparisonFunc::kLess;

  if (desc.compile_depth_off) {
    program->depth_off_ = program->make_graphics_pso(
        color_format, depth_format, false, depth_off, blend);
    if (!program->depth_off_) {
      return nullptr;
    }
    // Second variant: same disabled depth state but DS format attached so
    // sky/fog draws into shared-depth passes are not dropped by D3D12.
    program->depth_off_ds_ = program->make_graphics_pso(
        color_format, depth_format, true, depth_off, blend);
    if (!program->depth_off_ds_) {
      return nullptr;
    }
  }
  if (desc.compile_depth_write) {
    program->depth_write_ = program->make_graphics_pso(
        color_format, depth_format, true, depth_write, blend);
    if (!program->depth_write_) {
      return nullptr;
    }
  }
  if (desc.compile_depth_test) {
    program->depth_test_ = program->make_graphics_pso(
        color_format, depth_format, true, depth_test, blend);
    if (!program->depth_test_) {
      return nullptr;
    }
  }
  return program;
}

std::unique_ptr<FlycubeProgram> FlycubeProgram::compile_compute(
    const std::shared_ptr<::Device>& device, const ComputePipelineDesc& desc) {
  if (!device) {
    return nullptr;
  }
  auto program = std::unique_ptr<FlycubeProgram>(new FlycubeProgram());
  program->device_ = device;
  program->compute_ = true;
  program->camera_slot_ = -1;
  if (!program->compile_shader(desc.compute, ::ShaderType::kCompute, "cs",
                               &program->cs_) ||
      !program->attach_bindings(desc.bindings, desc.binding_count)) {
    return nullptr;
  }
  ::ComputePipelineDesc pso;
  pso.shader = program->cs_;
  pso.layout = program->layout_;
  program->compute_pipeline_ = device->CreateComputePipeline(pso);
  if (!program->compute_pipeline_) {
    return nullptr;
  }
  return program;
}

bool FlycubeProgram::replay_draw(::CommandList* list, const Draw& draw,
                                 bool pass_depth, bool* sampled) {
  if (sampled) {
    *sampled = false;
  }
  if (!list || compute_ || !draw.vertex || !draw.index || draw.index_count == 0) {
    LOGGING(LOG_WARNING,
            "rhi.flycube replay_draw skip: bad args idx=%u vtx=%p ib=%p",
            draw.index_count, draw.vertex, draw.index);
    return false;
  }
  std::shared_ptr<::Pipeline> pso = select_depth(pass_depth, draw.depth);
  if (!pso) {
    LOGGING(LOG_WARNING,
            "rhi.flycube replay_draw skip: no PSO pass_depth=%d mode=%d",
            pass_depth ? 1 : 0, static_cast<int>(draw.depth));
    return false;
  }
  std::vector<std::pair<uint32_t, std::shared_ptr<::View>>> cb_views;
  cb_views.reserve(draw.constants.size() + 1);
  for (const ConstantBytes& constant : draw.constants) {
    std::shared_ptr<::View> view = take_cb_view(
        constant.slot, constant.bytes.data(),
        static_cast<uint32_t>(constant.bytes.size()));
    if (view) {
      cb_views.emplace_back(constant.slot, std::move(view));
    }
  }
  if (camera_slot_ >= 0) {
    float block[32];
    std::memcpy(block, draw.camera_view, sizeof(float) * 16);
    std::memcpy(block + 16, draw.camera_proj, sizeof(float) * 16);
    std::shared_ptr<::View> view = take_cb_view(
        static_cast<uint32_t>(camera_slot_), block,
        static_cast<uint32_t>(sizeof(block)));
    if (view) {
      cb_views.emplace_back(static_cast<uint32_t>(camera_slot_),
                            std::move(view));
    }
  }
  // Graphics textures are SRVs. UAV slots stay empty on this path.
  std::shared_ptr<::BindingSet> set =
      make_binding_set(draw.textures, {}, &cb_views);
  if (!set) {
    LOGGING(LOG_WARNING,
            "rhi.flycube replay_draw skip: binding set null tex_binds=%zu",
            draw.textures.size());
    return false;
  }
  // Keep alive until begin_frame_bindings (post-fence). See program.h.
  frame_binding_sets_.push_back(set);
  list->BindPipeline(pso);
  list->BindBindingSet(set);
  list->IASetVertexBuffer(0, draw.vertex->shared(), draw.vb_offset);
  list->IASetIndexBuffer(draw.index->shared(), draw.ib_offset,
                         gli::format::FORMAT_R32_UINT_PACK32);
  list->DrawIndexed(draw.index_count, draw.instance_count, draw.first_index,
                    draw.vertex_offset, draw.first_instance);
  if (sampled) {
    for (const SlotBinding& binding : bindings_) {
      if (binding.kind == BindingKind::kSrv &&
          find_texture(draw.textures, binding.slot) != nullptr) {
        *sampled = true;
        break;
      }
    }
  }
  return true;
}

bool FlycubeProgram::replay_dispatch(::CommandList* list, const Dispatch& item) {
  if (!list || !compute_ || !compute_pipeline_) {
    return false;
  }
  std::vector<std::pair<uint32_t, std::shared_ptr<::View>>> cb_views;
  cb_views.reserve(item.constants.size());
  for (const ConstantBytes& constant : item.constants) {
    std::shared_ptr<::View> view = take_cb_view(
        constant.slot, constant.bytes.data(),
        static_cast<uint32_t>(constant.bytes.size()));
    if (view) {
      cb_views.emplace_back(constant.slot, std::move(view));
    }
  }

  for (const SlotBinding& binding : bindings_) {
    if (binding.kind == BindingKind::kUav) {
      GpuTexture* tex = find_texture(item.uavs, binding.slot);
      if (tex && tex->shared()) {
        tex->barrier_to(list, ResourceState::kUnorderedAccess);
      }
    } else if (binding.kind == BindingKind::kSrv) {
      GpuTexture* tex = find_texture(item.srvs, binding.slot);
      if (tex && tex->shared()) {
        tex->barrier_to(list, ResourceState::kNonPixelShaderResource);
      }
    }
  }

  std::shared_ptr<::BindingSet> set =
      make_binding_set(item.srvs, item.uavs, &cb_views);
  if (!set) {
    return false;
  }
  frame_binding_sets_.push_back(set);
  list->BindPipeline(compute_pipeline_);
  list->BindBindingSet(set);
  list->Dispatch(item.groups_x, item.groups_y, item.groups_z);

  // Slot 0 is the UAV later read as an SRV (height / spectrum). Other UAV
  // slots only get a UAV barrier so they stay writable, matching the old
  // ocean FFT replay.
  if (item.barrier_after) {
    for (const SlotBinding& binding : bindings_) {
      if (binding.kind != BindingKind::kUav) {
        continue;
      }
      GpuTexture* tex = find_texture(item.uavs, binding.slot);
      if (!tex || !tex->shared()) {
        continue;
      }
      list->UAVResourceBarrier(tex->shared());
      if (binding.slot == 0) {
        tex->barrier_to(list, ResourceState::kAllShaderResource);
      }
    }
  }
  return true;
}

#endif  // HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render
