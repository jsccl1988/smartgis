// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU map IR: MapIR meshes in the view CRS. No RHI, HWND, or
// CameraMatrices. Colors are 0xAARRGGBB, matching ResolvedPaint.

#ifndef VISTA_COMPONENT_MAP_DRAW_H_
#define VISTA_COMPONENT_MAP_DRAW_H_

#include <cstdint>
#include <string>
#include <vector>

namespace vista {

// One mesh vertex. u/v are 0 for fill, line, circle, and text.
struct Vertex {
  float x = 0, y = 0, z = 0;
  float u = 0, v = 0;
};

enum class DrawKind : uint8_t {
  kRaster,
  kFill,
  kLine,
  kCircle,
  kIcon,
  kText,
};

// Raster composite. Lives here so this header does not include render/rhi.
enum class DrawBlend : uint8_t { kOver, kMultiply };

// GB/T 20257 annotation faces. Rasterizer maps these to 等线 / 左斜宋.
enum class MapFont : uint8_t {
  kGothic = 0,        // 中等线
  kGothicBold = 1,    // 粗等线
  kGothicLight = 2,   // 细等线
  kSongOblique = 3,   // 左斜宋（水系）
};

// One painter primitive. kRaster stores TileSlot::texture_key in codepoint.
struct DrawItem {
  DrawKind kind = DrawKind::kFill;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  uint32_t rgba = 0xffffffff;
  float opacity = 1.f;
  float angle_rad = 0.f;
  uint32_t codepoint = 0;
  float text_size_px = 0.f;
  MapFont font = MapFont::kGothic;
  float halo_width_px = 0.f;
  uint32_t halo_rgba = 0;
  std::string symbol_id;
  bool pixel_space = false;
  // Pixel-space pivot for angle_rad. World-space items leave these at 0.
  float anchor_x = 0.f;
  float anchor_y = 0.f;
  // Screen HUD (scale bar / north arrow): always re-place for the live view.
  // Unlike geo labels, these must not ride DrawCache pan reuse or they drift.
  bool screen_fixed = false;
  DrawBlend blend = DrawBlend::kOver;
  // C1 per-layer DrawItem cache: source × zoom × viewport tile (0 = untagged).
  uint64_t cache_key = 0;
};

// Host texture id for heatmap density kRaster ('HEAT'). Matches content
// kMap2dHeatmapTextureKey — load_raster must serve this key.
inline constexpr uint32_t kHeatmapTextureKey = 0x48454154u;

// One CPU frame. Background is a clear, not a mesh.
struct MapIR {
  // Product carto ocean (#aad3df). Keep this as the no-style clear so jet
  // hillshade transparent ocean cells composite over water, not black.
  uint32_t background_rgba = 0xffaad3df;
  float background_opacity = 1.f;
  std::vector<DrawItem> items;
};

}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_DRAW_H_
