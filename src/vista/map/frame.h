// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU map frame umbrella. gis does not include render. No RHI, HWND, or
// CameraMatrices. MapFrame meshes are in the view CRS. Ortho is the
// current camera; perspective uses this same frame and Layout with a
// camera supplied by the host. The directory is vista/map (CPU layout).
// The namespace is vista.
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

#ifndef VISTA_MAP_FRAME_H_
#define VISTA_MAP_FRAME_H_

#include "vista/map/batch.h"
#include "vista/map/draw.h"
#include "vista/map/layout.h"
#include "vista/map/view.h"

#endif  // VISTA_MAP_FRAME_H_
