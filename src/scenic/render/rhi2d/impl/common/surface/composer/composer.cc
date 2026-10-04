// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/surface/composer/composer.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "gpu/compositor/composer/software_blend.h"

#pragma comment(lib, "Msimg32.lib")

namespace scenic {
namespace detail {
namespace {

uint32_t dib_stride(int width) { return static_cast<uint32_t>(width) * 4u; }

bool copy_region_bgra(const uint8_t* src_bits, uint32_t src_stride, int src_x,
                      int src_y, int w, int h, std::vector<uint8_t>* out) {
  if (!src_bits || !out || w < 1 || h < 1) {
    return false;
  }
  out->resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
  for (int y = 0; y < h; ++y) {
    const uint8_t* row = src_bits +
                         static_cast<size_t>(src_y + y) * src_stride +
                         static_cast<size_t>(src_x) * 4u;
    uint8_t* dst =
        out->data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
    std::memcpy(dst, row, static_cast<size_t>(w) * 4u);
  }
  return true;
}

// Nearest-neighbor scale. Matches StretchBlt COLORONCOLOR for opaque maps.
bool stretch_bgra_nn(const uint8_t* src, uint32_t src_stride, int src_w,
                     int src_h, int dest_w, int dest_h,
                     std::vector<uint8_t>* out) {
  if (!src || !out || src_w < 1 || src_h < 1 || dest_w < 1 || dest_h < 1) {
    return false;
  }
  if (src_w == dest_w && src_h == dest_h) {
    out->resize(static_cast<size_t>(dest_w) * static_cast<size_t>(dest_h) * 4u);
    for (int y = 0; y < dest_h; ++y) {
      std::memcpy(out->data() + static_cast<size_t>(y) * dest_w * 4u,
                  src + static_cast<size_t>(y) * src_stride,
                  static_cast<size_t>(dest_w) * 4u);
    }
    return true;
  }
  out->resize(static_cast<size_t>(dest_w) * static_cast<size_t>(dest_h) * 4u);
  for (int y = 0; y < dest_h; ++y) {
    const int sy = y * src_h / dest_h;
    const uint8_t* srow = src + static_cast<size_t>(sy) * src_stride;
    uint8_t* drow =
        out->data() + static_cast<size_t>(y) * static_cast<size_t>(dest_w) * 4u;
    for (int x = 0; x < dest_w; ++x) {
      const int sx = x * src_w / dest_w;
      const uint8_t* sp = srow + static_cast<size_t>(sx) * 4u;
      uint8_t* dp = drow + static_cast<size_t>(x) * 4u;
      dp[0] = sp[0];
      dp[1] = sp[1];
      dp[2] = sp[2];
      dp[3] = sp[3];
    }
  }
  return true;
}

void apply_color_key_alpha(std::vector<uint8_t>* bgra, COLORREF key) {
  if (!bgra) {
    return;
  }
  const uint8_t kr = GetRValue(key);
  const uint8_t kg = GetGValue(key);
  const uint8_t kb = GetBValue(key);
  for (size_t i = 0; i + 3 < bgra->size(); i += 4) {
    uint8_t* p = bgra->data() + i;
    if (p[0] == kb && p[1] == kg && p[2] == kr) {
      p[3] = 0;
    } else if (p[3] == 0) {
      p[3] = 255;
    }
  }
}

bool write_region_bgra(uint8_t* dst_bits, uint32_t dst_stride, int dest_x,
                       int dest_y, int w, int h, const uint8_t* src,
                       uint32_t src_stride) {
  if (!dst_bits || !src || w < 1 || h < 1) {
    return false;
  }
  for (int y = 0; y < h; ++y) {
    uint8_t* row = dst_bits + static_cast<size_t>(dest_y + y) * dst_stride +
                   static_cast<size_t>(dest_x) * 4u;
    const uint8_t* sp = src + static_cast<size_t>(y) * src_stride;
    std::memcpy(row, sp, static_cast<size_t>(w) * 4u);
  }
  return true;
}

}  // namespace

bool blit_surfaces(Rhi2dSurface& src, Rhi2dSurface& dst, int dest_x, int dest_y,
                   int dest_w, int dest_h, int src_x, int src_y, int src_w,
                   int src_h, bool color_key, COLORREF key) {
  if (!src.bits || !dst.bits || dest_w < 1 || dest_h < 1 || src_w < 1 ||
      src_h < 1) {
    return false;
  }
  if (src_x < 0 || src_y < 0 || dest_x < 0 || dest_y < 0) {
    return false;
  }
  if (src_x + src_w > src.width || src_y + src_h > src.height) {
    return false;
  }
  if (dest_x + dest_w > dst.width || dest_y + dest_h > dst.height) {
    return false;
  }
  // Guard against pathological sizes (corrupt viewport floats → OOM abort).
  constexpr int kMaxComposeEdge = 16384;
  constexpr int64_t kMaxComposePixels = 64LL * 1024LL * 1024LL;
  if (dest_w > kMaxComposeEdge || dest_h > kMaxComposeEdge ||
      src_w > kMaxComposeEdge || src_h > kMaxComposeEdge) {
    return false;
  }
  if (static_cast<int64_t>(dest_w) * dest_h > kMaxComposePixels ||
      static_cast<int64_t>(src_w) * src_h > kMaxComposePixels) {
    return false;
  }

  const uint32_t dst_stride = dst.stride_bytes;
  const uint32_t src_stride = src.stride_bytes;
  auto* dst_bits = static_cast<uint8_t*>(dst.bits);
  const auto* src_bits = static_cast<const uint8_t*>(src.bits);

  // Fast path: 1:1 color-key — copy non-key pixels in place (no temps /
  // float blend). Parallel layer play composes N full frames this way.
  if (color_key && src_w == dest_w && src_h == dest_h) {
    const uint8_t kr = GetRValue(key);
    const uint8_t kg = GetGValue(key);
    const uint8_t kb = GetBValue(key);
    for (int y = 0; y < dest_h; ++y) {
      const uint8_t* s = src_bits +
                         static_cast<size_t>(src_y + y) * src_stride +
                         static_cast<size_t>(src_x) * 4u;
      uint8_t* d = dst_bits + static_cast<size_t>(dest_y + y) * dst_stride +
                   static_cast<size_t>(dest_x) * 4u;
      for (int x = 0; x < dest_w; ++x) {
        if (!(s[0] == kb && s[1] == kg && s[2] == kr)) {
          d[0] = s[0];
          d[1] = s[1];
          d[2] = s[2];
          d[3] = 255;
        }
        s += 4;
        d += 4;
      }
    }
    dst.bump_generation();
    dst.mark_dirty(dest_x, dest_y, dest_w, dest_h);
    return true;
  }

  std::vector<uint8_t> src_patch;
  if (!copy_region_bgra(src_bits, src_stride, src_x, src_y, src_w, src_h,
                        &src_patch)) {
    return false;
  }
  if (color_key) {
    apply_color_key_alpha(&src_patch, key);
  } else {
    for (size_t i = 3; i < src_patch.size(); i += 4) {
      src_patch[i] = 255;
    }
  }

  std::vector<uint8_t> src_region;
  if (!stretch_bgra_nn(src_patch.data(), dib_stride(src_w), src_w, src_h,
                       dest_w, dest_h, &src_region)) {
    return false;
  }

  std::vector<uint8_t> dest_region;
  if (!copy_region_bgra(dst_bits, dst_stride, dest_x, dest_y, dest_w, dest_h,
                        &dest_region)) {
    return false;
  }

  gpu::detail::RenderPass pass;
  pass.width_px = static_cast<uint32_t>(dest_w);
  pass.height_px = static_cast<uint32_t>(dest_h);

  gpu::detail::DrawQuad base;
  base.material = gpu::detail::QuadMaterial::kBgra;
  base.x = 0;
  base.y = 0;
  base.w = dest_w;
  base.h = dest_h;
  base.opacity = 1.f;
  base.replaces = true;
  base.bgra = dest_region.data();
  base.stride_bytes = dib_stride(dest_w);
  pass.quad_list.push_back(base);

  gpu::detail::DrawQuad overlay;
  overlay.material = gpu::detail::QuadMaterial::kBgra;
  overlay.x = 0;
  overlay.y = 0;
  overlay.w = dest_w;
  overlay.h = dest_h;
  overlay.opacity = 1.f;
  overlay.replaces = !color_key;
  overlay.bgra = src_region.data();
  overlay.stride_bytes = dib_stride(dest_w);
  pass.quad_list.push_back(overlay);

  std::vector<uint8_t> composed;
  if (!gpu::detail::blend_render_pass(pass, pass.width_px, pass.height_px,
                                      &composed)) {
    return false;
  }
  if (!write_region_bgra(dst_bits, dst_stride, dest_x, dest_y, dest_w, dest_h,
                         composed.data(), dib_stride(dest_w))) {
    return false;
  }
  dst.bump_generation();
  dst.mark_dirty(dest_x, dest_y, dest_w, dest_h);
  return true;
}

bool make_compositor_frame(const Rhi2dSurface& buf,
                           gpu::detail::CompositorFrame* out) {
  if (!out || !buf.bits || buf.width < 1 || buf.height < 1) {
    return false;
  }
  const uint32_t w = static_cast<uint32_t>(buf.width);
  const uint32_t h = static_cast<uint32_t>(buf.height);
  const uint32_t stride = buf.stride_bytes;
  const auto* bits = static_cast<const uint8_t*>(buf.bits);
  std::vector<uint8_t> copy(static_cast<size_t>(w) * h * 4u);
  for (uint32_t y = 0; y < h; ++y) {
    std::memcpy(copy.data() + static_cast<size_t>(y) * w * 4u,
                bits + static_cast<size_t>(y) * stride,
                static_cast<size_t>(w) * 4u);
  }

  out->width_px = w;
  out->height_px = h;
  out->render_pass_list.clear();
  gpu::detail::RenderPass pass;
  pass.width_px = w;
  pass.height_px = h;
  gpu::detail::append_bgra_quad(&pass, std::move(copy), 1.f, /*replaces=*/true,
                                /*texture_cache_key=*/0);
  out->render_pass_list.push_back(std::move(pass));
  return true;
}

bool blend_frame_to_bgra(const gpu::detail::CompositorFrame& frame,
                         std::vector<uint8_t>* out_bgra) {
  if (!out_bgra || frame.width_px == 0 || frame.height_px == 0 ||
      frame.render_pass_list.empty()) {
    return false;
  }
  return gpu::detail::blend_render_pass(
      frame.render_pass_list.back(), frame.width_px, frame.height_px, out_bgra);
}

bool submit_surface(const Rhi2dSurface& buf) {
  BASE_TRACE_EVENT("compose", "gdi.frame");
  const CompositorSubmitSinks sinks = snapshot_compositor_submit();
  if (!sinks.frame_fn && !sinks.bgra_fn) {
    return false;
  }
  gpu::detail::CompositorFrame frame;
  if (!make_compositor_frame(buf, &frame)) {
    return false;
  }
  bool ok = false;
  if (sinks.frame_fn) {
    ok = sinks.frame_fn(frame) || ok;
  }
  if (sinks.bgra_fn) {
    std::vector<uint8_t> pixels;
    if (blend_frame_to_bgra(frame, &pixels)) {
      ok = sinks.bgra_fn(pixels.data(), frame.width_px, frame.height_px,
                         frame.width_px * 4u, sinks.bgra_user) ||
           ok;
    }
  }
  return ok;
}

long present_to_hwnd(Rhi2dOwnedSurface& src, int dest_org_x, int dest_org_y,
                     int dest_w, int dest_h, int src_org_x, int src_org_y,
                     int op) {
  return present_to_hwnd(src, dest_org_x, dest_org_y, dest_w, dest_h, src_org_x,
                         src_org_y, dest_w, dest_h, Rhi2dBlitMode::kOpaque, op,
                         RGB(255, 255, 255));
}

long present_to_hwnd(Rhi2dOwnedSurface& src, int dest_org_x, int dest_org_y,
                     int dest_w, int dest_h, int src_org_x, int src_org_y,
                     int src_w, int src_h, Rhi2dBlitMode mode, int op,
                     COLORREF clr) {
  if (src.bitmap() == nullptr) {
    return kErrFailure;
  }
  HDC hdc = GetDC(src.wnd());
  HDC src_dc = src.prepare_dc(false);
  BOOL ok = TRUE;
  switch (mode) {
    case Rhi2dBlitMode::kOpaque:
      ok = ::StretchBlt(hdc, dest_org_x, dest_org_y, dest_w, dest_h, src_dc,
                        src_org_x, src_org_y, src_w, src_h, op);
      break;
    case Rhi2dBlitMode::kColorKey:
      // HWND present stays GDI TransparentBlt (color-key). Layer compose uses
      // soft blit in blit_owned_to.
      ok = ::TransparentBlt(hdc, dest_org_x, dest_org_y, dest_w, dest_h, src_dc,
                            src_org_x, src_org_y, src_w, src_h, clr);
      break;
  }
  src.end_dc();
  ::ReleaseDC(src.wnd(), hdc);
  return ok ? kErrNone : kErrFailure;
}

long blit_owned_to(Rhi2dOwnedSurface& src, Rhi2dOwnedSurface& target,
                   int dest_org_x, int dest_org_y, int dest_w, int dest_h,
                   int src_org_x, int src_org_y, int op) {
  if (src.bitmap() == nullptr) {
    return kErrFailure;
  }

  // Opaque 1:1 copy: GDI BitBlt. Soft compose here only burned CPU and
  // diverged from the pre-strangler present path (look regression).
  HDC target_dc = target.prepare_dc(false);
  HDC src_dc = src.prepare_dc(false);
  const BOOL ok = ::BitBlt(target_dc, dest_org_x, dest_org_y, dest_w, dest_h,
                           src_dc, src_org_x, src_org_y, op);
  src.end_dc();
  target.end_dc();
  if (!ok) {
    return kErrFailure;
  }
  target.surface().bump_generation();
  target.surface().mark_dirty(dest_org_x, dest_org_y, dest_w, dest_h);
  return kErrNone;
}

long blit_owned_to(Rhi2dOwnedSurface& src, Rhi2dOwnedSurface& target,
                   int dest_org_x, int dest_org_y, int dest_w, int dest_h,
                   int src_org_x, int src_org_y, int src_w, int src_h,
                   Rhi2dBlitMode mode, int op, COLORREF clr) {
  if (src.bitmap() == nullptr) {
    return kErrFailure;
  }

  const bool need_scale = (src_w != dest_w) || (src_h != dest_h);
  const bool color_key = (mode == Rhi2dBlitMode::kColorKey);
  // Soft compose is NN stretch + multi-buffer temps. Map Refresh/publish
  // used it for every StretchBlt — worse carto + wheel-settle OOM/exit.
  // Keep CPU compose only for 1:1 color-key overlays; stretch/opaque → GDI.
  if (color_key && !need_scale &&
      blit_surfaces(src.surface(), target.surface(), dest_org_x, dest_org_y,
                    dest_w, dest_h, src_org_x, src_org_y, src_w, src_h,
                    /*color_key=*/true, clr)) {
    return kErrNone;
  }

  HDC target_dc = target.prepare_dc(false);
  HDC src_dc = src.prepare_dc(false);
  BOOL ok = TRUE;
  switch (mode) {
    case Rhi2dBlitMode::kOpaque:
      ok = ::StretchBlt(target_dc, dest_org_x, dest_org_y, dest_w, dest_h,
                        src_dc, src_org_x, src_org_y, src_w, src_h, op);
      break;
    case Rhi2dBlitMode::kColorKey:
      ok = ::TransparentBlt(target_dc, dest_org_x, dest_org_y, dest_w, dest_h,
                            src_dc, src_org_x, src_org_y, src_w, src_h, clr);
      break;
  }
  src.end_dc();
  target.end_dc();
  if (!ok) {
    return kErrFailure;
  }
  target.surface().bump_generation();
  target.surface().mark_dirty(dest_org_x, dest_org_y, dest_w, dest_h);
  return kErrNone;
}

}  // namespace detail
}  // namespace scenic
