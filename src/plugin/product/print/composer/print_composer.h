// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRINT_COMPOSER_PRINT_COMPOSER_H_
#define PLUGIN_PRINT_COMPOSER_PRINT_COMPOSER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace plugin {

// One legend row drawn in the page margin.
struct PrintLegendEntry {
  std::string label;
  uint32_t rgba = 0xff3366aa;  // 0xAARRGGBB swatch
};

// Inputs for a single-page layout (map + scale bar + legend).
struct PrintComposerInput {
  int page_width_px = 800;
  int page_height_px = 600;
  // Optional map pixels (BGRA bottom-up like GDI DIB). Empty → cream map panel.
  const uint8_t* map_bgra = nullptr;
  int map_width_px = 0;
  int map_height_px = 0;
  int map_stride_bytes = 0;
  // Map CRS units per pixel in the map panel (for scale bar length).
  double map_units_per_px = 1.0;
  std::string scale_label;  // e.g. "1:10000"; empty → auto from units_per_px
  std::vector<PrintLegendEntry> legend;
};

// Minimal Views-independent page compositor for print export.
class PrintComposer {
 public:
  // Compose |in| into |out_bgra| (32-bit BGRA, bottom-up, |out_stride|).
  // Returns false on bad input sizes.
  static bool compose(const PrintComposerInput& in,
                      std::vector<uint8_t>* out_bgra, int* out_w, int* out_h,
                      int* out_stride);

  // Compose and write a 32-bit BMP. PNG path aliases to BMP for P0.
  static bool export_page_bmp(const PrintComposerInput& in,
                              const std::string& path);
};

}  // namespace plugin

#endif  // PLUGIN_PRINT_COMPOSER_PRINT_COMPOSER_H_
