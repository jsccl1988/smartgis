// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/compositor/composer/software_composer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace gpu {
namespace detail {
namespace {

float clamp01(float v) {
  return (std::max)(0.f, (std::min)(1.f, v));
}

// Same src-over as the previous basemap blend_over, including the opaque
// coverage skip when opacity is 1 and source alpha is 255.
void blend_pixel(uint8_t* dst, uint8_t sb, uint8_t sg, uint8_t sr, uint8_t sa,
                 float op, bool opaque_op) {
  if (opaque_op && sa == 255) {
    dst[0] = sb;
    dst[1] = sg;
    dst[2] = sr;
    dst[3] = 255;
    return;
  }
  const float fb = sb / 255.f;
  const float fg = sg / 255.f;
  const float fr = sr / 255.f;
  const float fa = (sa / 255.f) * op;
  const float db = dst[0] / 255.f;
  const float dg = dst[1] / 255.f;
  const float dr = dst[2] / 255.f;
  const float da = dst[3] / 255.f;
  const float out_a = fa + da * (1.f - fa);
  float out_b = 0.f;
  float out_g = 0.f;
  float out_r = 0.f;
  if (out_a > 0.f) {
    out_b = (fb * fa + db * da * (1.f - fa)) / out_a;
    out_g = (fg * fa + dg * da * (1.f - fa)) / out_a;
    out_r = (fr * fa + dr * da * (1.f - fa)) / out_a;
  }
  dst[0] = static_cast<uint8_t>(std::lround(out_b * 255.f));
  dst[1] = static_cast<uint8_t>(std::lround(out_g * 255.f));
  dst[2] = static_cast<uint8_t>(std::lround(out_r * 255.f));
  dst[3] = static_cast<uint8_t>(std::lround(out_a * 255.f));
}

void blit_replaces(std::vector<uint8_t>* dst, uint32_t width, uint32_t height,
                   const DrawQuad& quad) {
  if (!dst || !quad.bgra || quad.stride_bytes == 0) {
    return;
  }
  int x0 = quad.x;
  int y0 = quad.y;
  int x1 = quad.x + quad.w;
  int y1 = quad.y + quad.h;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 > static_cast<int>(width)) {
    x1 = static_cast<int>(width);
  }
  if (y1 > static_cast<int>(height)) {
    y1 = static_cast<int>(height);
  }
  for (int y = y0; y < y1; ++y) {
    const uint8_t* src = quad.bgra +
                         static_cast<size_t>(y - quad.y) * quad.stride_bytes +
                         static_cast<size_t>(x0 - quad.x) * 4u;
    uint8_t* row = dst->data() + (static_cast<size_t>(y) * width +
                                  static_cast<size_t>(x0)) *
                                     4u;
    std::memcpy(row, src, static_cast<size_t>(x1 - x0) * 4u);
  }
}

void blend_quad(std::vector<uint8_t>* dst, uint32_t width, uint32_t height,
                const DrawQuad& quad) {
  if (!dst) {
    return;
  }
  if (quad.replaces && quad.material == QuadMaterial::kBgra) {
    blit_replaces(dst, width, height, quad);
    return;
  }
  const float op = clamp01(quad.opacity);
  const bool opaque_op = op == 1.f;
  int x0 = quad.x;
  int y0 = quad.y;
  int x1 = quad.x + quad.w;
  int y1 = quad.y + quad.h;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 > static_cast<int>(width)) {
    x1 = static_cast<int>(width);
  }
  if (y1 > static_cast<int>(height)) {
    y1 = static_cast<int>(height);
  }
  if (x0 >= x1 || y0 >= y1) {
    return;
  }

  if (quad.material == QuadMaterial::kSolid) {
    const uint8_t a = static_cast<uint8_t>((quad.argb >> 24) & 0xFF);
    const uint8_t r = static_cast<uint8_t>((quad.argb >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((quad.argb >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(quad.argb & 0xFF);
    for (int y = y0; y < y1; ++y) {
      uint8_t* row = dst->data() + static_cast<size_t>(y) * width * 4u;
      for (int x = x0; x < x1; ++x) {
        blend_pixel(row + static_cast<size_t>(x) * 4u, b, g, r, a, op,
                    opaque_op);
      }
    }
    return;
  }

  if (!quad.bgra || quad.stride_bytes == 0) {
    return;
  }
  for (int y = y0; y < y1; ++y) {
    const uint8_t* src = quad.bgra +
                         static_cast<size_t>(y - quad.y) * quad.stride_bytes +
                         static_cast<size_t>(x0 - quad.x) * 4u;
    uint8_t* row = dst->data() + static_cast<size_t>(y) * width * 4u +
                   static_cast<size_t>(x0) * 4u;
    const int span = x1 - x0;
    for (int x = 0; x < span; ++x) {
      const uint8_t* sp = src + static_cast<size_t>(x) * 4u;
      blend_pixel(row + static_cast<size_t>(x) * 4u, sp[0], sp[1], sp[2], sp[3],
                  op, opaque_op);
    }
  }
}

}  // namespace

bool blend_render_pass(const RenderPass& pass, uint32_t width_px,
                       uint32_t height_px, std::vector<uint8_t>* dst) {
  if (!dst || width_px == 0 || height_px == 0) {
    return false;
  }
  dst->assign(static_cast<size_t>(width_px) * height_px * 4u, 0);
  for (const DrawQuad& quad : pass.quad_list) {
    blend_quad(dst, width_px, height_px, quad);
  }
  return true;
}

bool SoftwareComposer::draw_frame(OutputSurface* surface,
                                  const CompositorFrame& frame) {
  if (!surface || frame.width_px == 0 || frame.height_px == 0 ||
      frame.render_pass_list.empty() ||
      frame.render_pass_list.back().quad_list.empty()) {
    return false;
  }
  std::vector<uint8_t> pixels;
  if (!blend_render_pass(frame.render_pass_list.back(), frame.width_px,
                         frame.height_px, &pixels)) {
    return false;
  }
  return surface->upload_bgra(pixels.data(), frame.width_px * 4u);
}

}  // namespace detail
}  // namespace gpu
