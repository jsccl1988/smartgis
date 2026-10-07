// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// View ortho envelope + vertex AABB for WorldPass rebuild.

#ifndef VISTA_COMPONENT_WORLD_SPACE_ENVELOPE_H_
#define VISTA_COMPONENT_WORLD_SPACE_ENVELOPE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/component/world/instance.h"

namespace vista {
namespace detail {

struct ViewOrtho {
  bool set = false;
  double min_x = 0;
  double min_y = 0;
  double max_x = 1;
  double max_y = 1;
};

void resolve_view_envelope(const ViewOrtho& ortho,
                           const std::vector<Instance>& instances,
                           uint32_t width, uint32_t height, double* min_x,
                           double* min_y, double* max_x, double* max_y);

void aabb_from_xyz(const float* positions, size_t float_count, float* min_x,
                   float* min_y, float* min_z, float* max_x, float* max_y,
                   float* max_z);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_SPACE_ENVELOPE_H_
