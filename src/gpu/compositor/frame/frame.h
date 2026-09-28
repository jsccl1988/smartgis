// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_COMPOSITOR_FRAME_FRAME_H_
#define GPU_COMPOSITOR_FRAME_FRAME_H_

#include <cstdint>
#include <vector>

// Recorded quads for one display frame. Included by display and the raster
// translation units. Not part of the frame_sink contract.

namespace gpu {
namespace detail {

enum class QuadMaterial { kSolid, kBgra };

// One quad in a render pass. Pixel rect is inclusive-exclusive, in output pixels.
struct DrawQuad {
  QuadMaterial material = QuadMaterial::kSolid;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  float opacity = 1.f;
  uint32_t argb = 0;  // kSolid, AARRGGBB
  const uint8_t* bgra = nullptr;  // kBgra, tightly packed; owned by RenderPass
  uint32_t stride_bytes = 0;
  // Finished bitmap (direct raster). Copied into the output instead of src-over.
  bool replaces = false;
  // Non-zero enables per-adapter RHI texture cache (M3). 0 = always upload.
  uint64_t texture_cache_key = 0;
};

// One ordered quad list, back to front. The root pass is the output.
struct RenderPass {
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  std::vector<DrawQuad> quad_list;
  // Backing store for kBgra quads. Pointers stay valid until this pass is
  // destroyed or image_data is cleared.
  std::vector<std::vector<uint8_t>> image_data;
};

// Recorded frame the GPU-process compositor executes onto OutputSurface.
// The last RenderPass is the root pass.
struct CompositorFrame {
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  std::vector<RenderPass> render_pass_list;
};

inline void append_solid_quad(RenderPass* pass, uint32_t argb, float opacity) {
  if (!pass) {
    return;
  }
  DrawQuad quad;
  quad.material = QuadMaterial::kSolid;
  quad.x = 0;
  quad.y = 0;
  quad.w = static_cast<int>(pass->width_px);
  quad.h = static_cast<int>(pass->height_px);
  quad.opacity = opacity;
  quad.argb = argb;
  pass->quad_list.push_back(quad);
}

inline void append_bgra_quad(RenderPass* pass, std::vector<uint8_t> bgra,
                             float opacity, bool replaces,
                             uint64_t texture_cache_key = 0) {
  if (!pass || bgra.empty()) {
    return;
  }
  pass->image_data.push_back(std::move(bgra));
  DrawQuad quad;
  quad.material = QuadMaterial::kBgra;
  quad.x = 0;
  quad.y = 0;
  quad.w = static_cast<int>(pass->width_px);
  quad.h = static_cast<int>(pass->height_px);
  quad.opacity = opacity;
  quad.replaces = replaces;
  quad.bgra = pass->image_data.back().data();
  quad.stride_bytes = pass->width_px * 4u;
  quad.texture_cache_key = texture_cache_key;
  pass->quad_list.push_back(quad);
}

// External BGRA (stable for the duration of draw_frame). Used when shell
// generation is unchanged so attach_shell_raster can skip a full memcpy.
inline void append_bgra_quad_external(RenderPass* pass, const uint8_t* bgra,
                                      uint32_t stride_bytes, float opacity,
                                      bool replaces,
                                      uint64_t texture_cache_key) {
  if (!pass || pass->width_px == 0 || pass->height_px == 0) {
    return;
  }
  if (!bgra && texture_cache_key == 0) {
    return;
  }
  DrawQuad quad;
  quad.material = QuadMaterial::kBgra;
  quad.x = 0;
  quad.y = 0;
  quad.w = static_cast<int>(pass->width_px);
  quad.h = static_cast<int>(pass->height_px);
  quad.opacity = opacity;
  quad.replaces = replaces;
  quad.bgra = bgra;
  quad.stride_bytes =
      stride_bytes != 0 ? stride_bytes : pass->width_px * 4u;
  quad.texture_cache_key = texture_cache_key;
  pass->quad_list.push_back(quad);
}

}  // namespace detail
}  // namespace gpu

#endif  // GPU_COMPOSITOR_FRAME_FRAME_H_
