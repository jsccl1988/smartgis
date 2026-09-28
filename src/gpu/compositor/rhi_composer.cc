// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/compositor/rhi_composer.h"

#include "gpu/compositor/rhi_underlay_bridge.h"
#include "gpu/compositor/software_renderer.h"
#include "gpu/device/gpu_device_hub.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace gpu {
namespace detail {
namespace {

constexpr uint32_t k_pos_stride = 3u * sizeof(float);
constexpr uint32_t k_uv_stride = 5u * sizeof(float);

void bgra_to_rgba(const uint8_t* bgra, uint32_t stride_bytes, int w, int h,
                  std::vector<uint8_t>* rgba) {
  rgba->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0);
  for (int y = 0; y < h; ++y) {
    const uint8_t* src = bgra + static_cast<size_t>(y) * stride_bytes;
    uint8_t* dst =
        rgba->data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
    for (int x = 0; x < w; ++x) {
      dst[0] = src[2];
      dst[1] = src[1];
      dst[2] = src[0];
      dst[3] = src[3];
      src += 4;
      dst += 4;
    }
  }
}

uint64_t hash_bgra_sample(const DrawQuad& quad) {
  if (quad.texture_cache_key != 0) {
    return quad.texture_cache_key;
  }
  if (!quad.bgra || quad.w <= 0 || quad.h <= 0 || quad.stride_bytes == 0) {
    return 0;
  }
  // Content fingerprint when callers omit an explicit key (fallback).
  uint64_t h = 14695981039346656037ull;
  const size_t nbytes =
      static_cast<size_t>(quad.h) * static_cast<size_t>(quad.stride_bytes);
  const size_t step = (std::max)(size_t{4}, nbytes / 64u);
  for (size_t i = 0; i < nbytes; i += step) {
    h ^= quad.bgra[i];
    h *= 1099511628211ull;
  }
  h ^= static_cast<uint64_t>(quad.w) << 32;
  h ^= static_cast<uint64_t>(quad.h);
  return h == 0 ? 1 : h;
}

bool upload_solid_quad(render::rhi::Device* device,
                       render::rhi::CommandList* list,
                       render::rhi::Pipeline* pipeline, const DrawQuad& quad,
                       float surface_w, float surface_h,
                       render::rhi::Buffer** keep_vb) {
  const float x0 = static_cast<float>(quad.x);
  const float y0 = static_cast<float>(quad.y);
  const float x1 = static_cast<float>(quad.x + quad.w);
  const float y1 = static_cast<float>(quad.y + quad.h);
  // Pixel space, y-down → ortho y-up: flip Y against surface height.
  const float verts[12] = {
      x0, surface_h - y0, 0.f, x1, surface_h - y0, 0.f,
      x1, surface_h - y1, 0.f, x0, surface_h - y1, 0.f,
  };
  render::rhi::Buffer* vb =
      device->create_buffer(sizeof(verts), render::rhi::BufferUsage::kVertex);
  if (!vb || !device->upload(vb, verts, sizeof(verts))) {
    device->destroy_buffer(vb);
    return false;
  }
  *keep_vb = vb;

  const float a = ((quad.argb >> 24) & 0xFF) / 255.f * quad.opacity;
  const float r = ((quad.argb >> 16) & 0xFF) / 255.f;
  const float g = ((quad.argb >> 8) & 0xFF) / 255.f;
  const float b = (quad.argb & 0xFF) / 255.f;
  list->bind_texture(nullptr, 0);
  list->set_pipeline(pipeline);
  list->set_blend_mode(quad.replaces || a >= 0.999f
                           ? render::rhi::BlendMode::kOpaque
                           : render::rhi::BlendMode::kSrcAlpha);
  const render::programs::Color color{r, g, b, a};
  list->set_constants(render::programs::kColorSlot, &color, sizeof(color));
  list->bind_vertex_buffer(vb, 0, k_pos_stride);
  return true;
}

bool upload_textured_quad(render::rhi::Device* device,
                          render::rhi::CommandList* list,
                          render::rhi::Pipeline* pipeline, const DrawQuad& quad,
                          render::rhi::Texture* texture, float surface_w,
                          float surface_h, render::rhi::Buffer** keep_vb) {
  if (!texture) {
    return false;
  }
  const float x0 = static_cast<float>(quad.x);
  const float y0 = static_cast<float>(quad.y);
  const float x1 = static_cast<float>(quad.x + quad.w);
  const float y1 = static_cast<float>(quad.y + quad.h);
  const float verts[20] = {
      x0, surface_h - y0, 0.f, 0.f, 0.f, x1, surface_h - y0, 0.f, 1.f, 0.f,
      x1, surface_h - y1, 0.f, 1.f, 1.f, x0, surface_h - y1, 0.f, 0.f, 1.f,
  };
  render::rhi::Buffer* vb =
      device->create_buffer(sizeof(verts), render::rhi::BufferUsage::kVertex);
  if (!vb || !device->upload(vb, verts, sizeof(verts))) {
    device->destroy_buffer(vb);
    return false;
  }
  *keep_vb = vb;
  list->bind_texture(texture, 0);
  list->set_pipeline(pipeline);
  list->set_blend_mode(quad.replaces ? render::rhi::BlendMode::kOpaque
                                     : render::rhi::BlendMode::kSrcAlpha);
  // kPsTextured multiplies by ColorCB tint; keep RGB white and put opacity in A.
  const float a =
      (!quad.replaces && quad.opacity < 0.999f) ? quad.opacity : 1.f;
  const render::programs::Color tint{1.f, 1.f, 1.f, a};
  list->set_constants(render::programs::kColorSlot, &tint, sizeof(tint));
  list->bind_vertex_buffer(vb, 0, k_uv_stride);
  return true;
}

}  // namespace

RhiComposer::RhiComposer(AdapterId adapter) : adapter_(adapter) {
  if (adapter_ == kAdapterInvalid) {
    adapter_ = kAdapterPrimary;
  }
}

RhiComposer::~RhiComposer() {
  if (!cached_device_) {
    return;
  }
  if (quad_ib_) {
    cached_device_->destroy_buffer(quad_ib_);
    quad_ib_ = nullptr;
  }
  if (solid_) {
    cached_device_->destroy_pipeline(solid_);
    solid_ = nullptr;
  }
  if (textured_) {
    cached_device_->destroy_pipeline(textured_);
    textured_ = nullptr;
  }
}

render::rhi::Texture* RhiComposer::texture_for_quad(
    render::rhi::Device* device, const DrawQuad& quad) {
  if (!device || quad.w <= 0 || quad.h <= 0) {
    return nullptr;
  }
  GpuDeviceHub& hub = device_hub();
  const uint64_t key = hash_bgra_sample(quad);
  if (key != 0) {
    if (render::rhi::Texture* hit = hub.find_cached_texture(adapter_, key)) {
      return hit;
    }
  }
  // Cache miss requires CPU pixels. Generation-only quads skip upload when
  // the texture is already cached (shell_generation unchanged).
  if (!quad.bgra || quad.stride_bytes == 0) {
    return nullptr;
  }
  std::vector<uint8_t> rgba;
  bgra_to_rgba(quad.bgra, quad.stride_bytes, quad.w, quad.h, &rgba);
  render::rhi::TextureDesc desc;
  desc.width = static_cast<uint32_t>(quad.w);
  desc.height = static_cast<uint32_t>(quad.h);
  desc.format = render::rhi::TextureFormat::kRgba8;
  desc.usage =
      render::rhi::TextureUsage::kSampled | render::rhi::TextureUsage::kCopyDest;
  render::rhi::Texture* tex = device->create_texture(desc);
  if (!tex ||
      !device->upload_texture(tex, rgba.data(),
                              static_cast<uint32_t>(rgba.size()))) {
    device->destroy_texture(tex);
    return nullptr;
  }
  if (key != 0) {
    hub.put_cached_texture(adapter_, key, tex);
  }
  return tex;
}

bool RhiComposer::record_gpu_compose(render::rhi::Device* device,
                                     const CompositorFrame& frame) {
  if (!device || frame.render_pass_list.empty()) {
    return false;
  }
  const RenderPass& pass = frame.render_pass_list.back();
  if (pass.quad_list.empty()) {
    return false;
  }

  if (!quad_ib_) {
    const uint32_t indices[6] = {0, 1, 2, 0, 2, 3};
    quad_ib_ =
        device->create_buffer(sizeof(indices), render::rhi::BufferUsage::kIndex);
    if (!quad_ib_ ||
        !device->upload(quad_ib_, indices, sizeof(indices))) {
      device->destroy_buffer(quad_ib_);
      quad_ib_ = nullptr;
      return false;
    }
  }

  render::rhi::CommandList* list = device->create_command_list();
  if (!list) {
    return false;
  }
  if (!solid_) {
    solid_ = device->create_graphics_pipeline(render::programs::solid_pipeline_desc());
  }
  if (!textured_) {
    textured_ =
        device->create_graphics_pipeline(render::programs::textured_pipeline_desc());
  }
  if (!solid_ || !textured_) {
    device->destroy_command_list(list);
    return false;
  }

  const float sw = static_cast<float>(frame.width_px);
  const float sh = static_cast<float>(frame.height_px);
  list->bind_camera(
      render::rhi::make_ortho_camera(0.f, sw, 0.f, sh, -1.f, 1.f));

  render::rhi::RenderPassDesc desc;
  desc.width = frame.width_px;
  desc.height = frame.height_px;
  desc.load_op = render::rhi::ColorLoadOp::kClear;
  desc.clear_r = 0.f;
  desc.clear_g = 0.f;
  desc.clear_b = 0.f;
  desc.clear_a = 0.f;
  list->begin_render_pass(desc);
  list->set_viewport(0.f, 0.f, sw, sh, 0.f, 1.f);
  list->set_depth_mode(render::rhi::DepthMode::kDisabled);

  std::vector<render::rhi::Buffer*> transient_vb;
  bool ok = true;
  for (const DrawQuad& quad : pass.quad_list) {
    if (quad.w <= 0 || quad.h <= 0) {
      continue;
    }
    render::rhi::Buffer* vb = nullptr;
    if (quad.material == QuadMaterial::kSolid) {
      if (!upload_solid_quad(device, list, solid_, quad, sw, sh, &vb)) {
        ok = false;
        break;
      }
    } else {
      render::rhi::Texture* tex = texture_for_quad(device, quad);
      if (!upload_textured_quad(device, list, textured_, quad, tex, sw, sh,
                                &vb)) {
        ok = false;
        break;
      }
    }
    if (vb) {
      transient_vb.push_back(vb);
    }
    list->bind_index_buffer(quad_ib_, 0);
    list->draw_indexed(6, 1, 0, 0, 0);
  }

  list->end_render_pass();
  list->close();

  if (ok) {
    ok = device->execute(list);
  }
  device->destroy_command_list(list);
  for (render::rhi::Buffer* vb : transient_vb) {
    device->destroy_buffer(vb);
  }
  return ok;
}

bool RhiComposer::present_to_surface(render::rhi::Device* device,
                                     OutputSurface* surface,
                                     const uint8_t* bgra, uint32_t width_px,
                                     uint32_t height_px) {
  if (!surface || !bgra || width_px == 0 || height_px == 0) {
    return false;
  }
  const uint32_t stride = width_px * 4u;

  // Prefer RHI import of the DXGI NT shared handle when the backend supports
  // it (FlyCube DX12 OpenSharedHandle + CopyBufferToTexture).
  if (device && surface->share_handle() && surface->import_ready()) {
    if (!device->has_imported_shared()) {
      (void)device->import_shared_nt_handle(surface->share_handle(), width_px,
                                            height_px);
    }
    if (device->has_imported_shared() &&
        device->copy_bgra_to_imported_shared(bgra, stride, width_px,
                                             height_px)) {
      return true;
    }
  }

  // Last resort: D3D11 Map/UpdateSubresource or DIB present on OutputSurface.
  return surface->upload_bgra(bgra, stride);
}

bool RhiComposer::draw_frame(OutputSurface* surface,
                             const CompositorFrame& frame) {
  if (!surface || frame.width_px == 0 || frame.height_px == 0 ||
      frame.render_pass_list.empty() ||
      frame.render_pass_list.back().quad_list.empty()) {
    return false;
  }

  GpuDeviceHub& hub = device_hub();
  render::rhi::Device* device = hub.ensure_rhi_device(adapter_);
  if (!device) {
    SoftwareComposer software(adapter_);
    return software.draw_frame(surface, frame);
  }

  if (cached_device_ != device) {
    if (cached_device_) {
      if (quad_ib_) {
        cached_device_->destroy_buffer(quad_ib_);
        quad_ib_ = nullptr;
      }
      if (solid_) {
        cached_device_->destroy_pipeline(solid_);
        solid_ = nullptr;
      }
      if (textured_) {
        cached_device_->destroy_pipeline(textured_);
        textured_ = nullptr;
      }
    }
    cached_device_ = device;
    cache_generation_ = hub.slot_generation(adapter_);
  } else if (hub.slot_generation(adapter_) != cache_generation_) {
    hub.clear_texture_cache(adapter_);
    cache_generation_ = hub.slot_generation(adapter_);
  }

  // Import the browser-facing DXGI NT shared texture when available so GPU
  // compose can target it directly (FlyCube DX12 OpenSharedHandle).
  if (surface->share_handle() && surface->import_ready()) {
    (void)device->import_shared_nt_handle(surface->share_handle(),
                                          frame.width_px, frame.height_px);
  }

  // M4: optional Frame Graph / GpuScene underlay on the same device first.
  (void)record_hub_underlay(adapter_, device, nullptr, frame.width_px,
                            frame.height_px);

  // M2: record GPU compose. When import succeeded, execute targets the shared
  // RT (no CPU upload). Otherwise FlyCube may drop graphics offscreen.
  const bool gpu_ok = record_gpu_compose(device, frame);
  if (gpu_ok && device->composed_into_imported_shared()) {
    return true;
  }
  if (!gpu_ok) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      std::fprintf(stderr,
                   "gpu: RhiComposer GPU compose record/execute failed on "
                   "adapter %u; presenting via CPU blend (still GPU process)\n",
                   static_cast<unsigned>(adapter_));
    }
  }

  std::vector<uint8_t> pixels;
  if (!blend_render_pass(frame.render_pass_list.back(), frame.width_px,
                         frame.height_px, &pixels)) {
    hub.set_software_fallback(adapter_, true);
    SoftwareComposer software(adapter_);
    return software.draw_frame(surface, frame);
  }

  if (!present_to_surface(device, surface, pixels.data(), frame.width_px,
                          frame.height_px)) {
    hub.set_software_fallback(adapter_, true);
    return surface->upload_bgra(pixels.data(), frame.width_px * 4u);
  }
  return true;
}

}  // namespace detail
}  // namespace gpu
