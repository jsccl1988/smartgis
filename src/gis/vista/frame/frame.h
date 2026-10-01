// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU map frame. gis does not include render. No RHI, HWND, or
// CameraMatrices. MapFrame meshes are in the view CRS. Ortho is the
// current camera; perspective uses this same frame and Layout with a
// camera supplied by the host. The directory is vista/frame. The
// namespace is gis::vista.
//
// Final signature (views and render call this):
//   MapFrame Layout::build(const LayoutInput& in,
//                           const std::vector<LayerBatch>& layers) const;
//   std::string default_carto_style_json();
//
// LayerBatch replaces a flat OGRFeature list so style source-layer matching
// stays explicit. LayoutInput::symbols is the icon / fill-pattern library
// (id + pixel size). A missing id skips the icon and still places text.
// DrawItem::codepoint on kRaster carries TileSlot::texture_key.
// DrawItem::anchor_x / anchor_y is the pixel-space rotation pivot for
// kIcon and kText. Colors are 0xAARRGGBB, matching ResolvedPaint.
// World items (fill, line, circle, raster) stay in the view CRS
// (pixel_space false). Raster quads use u/v 0..1 with v = 0 on the north
// edge (max_y). Icon and text stay screen HUD (pixel_space true,
// axis-aligned); the pass rotates them about the anchor by angle_rad.
// Vertex already carries an optional z in that same CRS.

#ifndef GIS_VISTA_FRAME_H_
#define GIS_VISTA_FRAME_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "gis/gis_export.h"
#include "gis/present/style/style_types.h"

class OGRGeometry;

namespace gis {
namespace vista {

// Host camera hint. Layout does not project from this; a map3d host sets
// kPerspective on the same View. There is no second Layout type.
enum class ViewMode : uint8_t {
  kOrtho,
  kPerspective,
};

// Viewport in pixels, the envelope in the view CRS, and the camera-mode hint.
struct View {
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  ViewMode mode = ViewMode::kOrtho;
};

// Injected glyph advances. Tests use a fixed-advance stub.
// Pixel rasterization stays in the render pass.
class GlyphMetrics {
 public:
  virtual ~GlyphMetrics() = default;
  virtual float advance_px(uint32_t codepoint, float text_size_px) const = 0;
};

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
  float halo_width_px = 0.f;
  uint32_t halo_rgba = 0;
  std::string symbol_id;
  bool pixel_space = false;
  // Pixel-space pivot for angle_rad. World-space items leave these at 0.
  float anchor_x = 0.f;
  float anchor_y = 0.f;
};

// One CPU frame. Background is a clear, not a mesh.
struct MapFrame {
  uint32_t background_rgba = 0xfff5f0e6;
  float background_opacity = 1.f;
  std::vector<DrawItem> items;
};

// One raster tile whose corners are already in the view CRS (lon/lat).
struct TileSlot {
  double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  float opacity = 1.f;
  uint32_t texture_key = 0;
};

// Pixel size of an icon or fill-pattern image the render pass can sample.
struct SymbolAsset {
  std::string id;
  float width_px = 0;
  float height_px = 0;
};

// Geometries that share one style source-layer, plus per-feature attributes.
struct LayerBatch {
  std::string source_layer;
  std::vector<const OGRGeometry*> geoms;
  std::vector<std::map<std::string, std::string>> attrs;
};

// Inputs that are not per-feature. symbols is matched by id.
// hillshade_tiles: host-baked DEM shade underlay (texture_key + lon/lat).
struct LayoutInput {
  View view;
  const gis::style::StyleDocument* style = nullptr;
  double zoom = 0;
  std::vector<TileSlot> tiles;
  std::vector<TileSlot> hillshade_tiles;
  const GlyphMetrics* metrics = nullptr;
  std::vector<SymbolAsset> symbols;
};

// Pure CPU layout: style order in, MapFrame out. No RHI, HWND, or
// CameraMatrices. View::mode is a host hint; this class does not branch on it.
class Layout {
 public:
  GIS_EXPORT MapFrame build(const LayoutInput& in,
                            const std::vector<LayerBatch>& layers) const;
};

GIS_EXPORT std::string default_carto_style_json();

}  // namespace vista
}  // namespace gis

#endif  // GIS_VISTA_FRAME_H_
