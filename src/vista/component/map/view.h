// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Viewport in pixels plus the envelope in the view CRS. Layout does not
// project from ViewMode; a map3d host sets kPerspective on the same View.

#ifndef VISTA_COMPONENT_MAP_VIEW_H_
#define VISTA_COMPONENT_MAP_VIEW_H_

#include <cstdint>

namespace vista {

enum class ViewMode : uint8_t {
  kOrtho,
  kPerspective,
};

struct View {
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  ViewMode mode = ViewMode::kOrtho;
};

}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_VIEW_H_
