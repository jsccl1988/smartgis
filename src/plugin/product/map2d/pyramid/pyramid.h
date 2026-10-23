// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Map2d presentation of one Web Mercator XYZ pyramid. Index math and fetch
// live in glang.runtime; this type only places tiles in the view and paints.

#ifndef PLUGIN_MAP2D_PYRAMID_PYRAMID_H_
#define PLUGIN_MAP2D_PYRAMID_PYRAMID_H_

#include "plugin/product/map2d/cartography/furniture.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

namespace ui {
namespace gfx {
class Canvas;
}
}  // namespace ui

namespace content {
class PluginHost;
}

namespace plugin {

struct PyramidCell {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int z = 0;
  int tx = 0;
  int ty = 0;
  bool has_image = false;
};

void bind_map_horizon(content::PluginHost* host);

// Process-lifetime chrome flags for the pyramid horizon. Defaults are on.
void set_map_furniture(const FurnitureOptions& options);
FurnitureOptions map_furniture();

// Template must contain {z}/{x}/{y}. Optional extent; otherwise the live
// GisContents extent or the last frame_to box is used.
bool open_pyramid(std::string_view args);

void note_view_extent(double west, double south, double east, double north);

// Viewport horizon paint hook. Reads the cache only; prefetch happens in open_pyramid.
void paint_map_horizon(HWND hwnd, HDC hdc, int width_px, int height_px);

}  // namespace plugin

#endif  // PLUGIN_MAP2D_PYRAMID_PYRAMID_H_
