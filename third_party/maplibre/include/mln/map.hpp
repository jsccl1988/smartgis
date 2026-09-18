// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef MLN_MAP_HPP_
#define MLN_MAP_HPP_

#include <cstdint>
#include <string>

// Embedder-facing still-image Map for Windows GPU Track A.
// Product chrome must not include this header.
// The fetched pin's real type is mln::Map in include/mln/map/map.hpp and
// needs RendererFrontend + HeadlessFrontend + a linked core. This facade
// constructs, loads style JSON, and paints BGRA background
// (background-color + background-opacity) without that stack or tile fetch.
// Multi-raster compositing stays in gpu/maplibre_adapter.
namespace mln {

class Map {
 public:
  Map(uint32_t width_px, uint32_t height_px);
  ~Map();

  Map(const Map&) = delete;
  Map& operator=(const Map&) = delete;

  void load_style_json(const char* json);
  bool render_still(uint8_t* bgra, uint32_t stride_bytes) const;
  bool constructed() const { return width_px_ != 0 && height_px_ != 0; }

 private:
  uint32_t width_px_ = 0;
  uint32_t height_px_ = 0;
  std::string style_json_;
};

}  // namespace mln

#endif  // MLN_MAP_HPP_
