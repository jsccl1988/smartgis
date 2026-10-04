// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/scene/envelope.h"

namespace vista {
namespace detail {

void resolve_view_envelope(const ViewOrtho& ortho,
                           const std::vector<GpuInstance>& instances,
                           uint32_t width, uint32_t height, double* min_x,
                           double* min_y, double* max_x, double* max_y) {
  double minx = 0;
  double miny = 0;
  double maxx = 1;
  double maxy = 1;
  bool have_box = false;
  if (ortho.set) {
    minx = ortho.min_x;
    miny = ortho.min_y;
    maxx = ortho.max_x;
    maxy = ortho.max_y;
    have_box = true;
  } else {
    for (const GpuInstance& inst : instances) {
      if (inst.kind != vista::NodeKind::kRasterLayer &&
          inst.kind != vista::NodeKind::kVectorLayer) {
        continue;
      }
      if (!have_box) {
        minx = inst.min_x;
        miny = inst.min_y;
        maxx = inst.max_x;
        maxy = inst.max_y;
        have_box = true;
      } else {
        if (inst.min_x < minx) {
          minx = inst.min_x;
        }
        if (inst.min_y < miny) {
          miny = inst.min_y;
        }
        if (inst.max_x > maxx) {
          maxx = inst.max_x;
        }
        if (inst.max_y > maxy) {
          maxy = inst.max_y;
        }
      }
    }
  }
  if (!have_box) {
    minx = 0;
    miny = 0;
    maxx = static_cast<double>(width);
    maxy = static_cast<double>(height);
  }
  if (maxx - minx < 1e-6) {
    maxx = minx + 1;
  }
  if (maxy - miny < 1e-6) {
    maxy = miny + 1;
  }
  if (min_x) {
    *min_x = minx;
  }
  if (min_y) {
    *min_y = miny;
  }
  if (max_x) {
    *max_x = maxx;
  }
  if (max_y) {
    *max_y = maxy;
  }
}

void aabb_from_xyz(const float* positions, size_t float_count, float* min_x,
                   float* min_y, float* min_z, float* max_x, float* max_y,
                   float* max_z) {
  if (!positions || float_count < 3) {
    return;
  }
  float mn_x = positions[0];
  float mn_y = positions[1];
  float mn_z = positions[2];
  float mx_x = mn_x;
  float mx_y = mn_y;
  float mx_z = mn_z;
  for (size_t i = 0; i + 2 < float_count; i += 3) {
    const float x = positions[i];
    const float y = positions[i + 1];
    const float z = positions[i + 2];
    if (x < mn_x) {
      mn_x = x;
    }
    if (y < mn_y) {
      mn_y = y;
    }
    if (z < mn_z) {
      mn_z = z;
    }
    if (x > mx_x) {
      mx_x = x;
    }
    if (y > mx_y) {
      mx_y = y;
    }
    if (z > mx_z) {
      mx_z = z;
    }
  }
  if (min_x) {
    *min_x = mn_x;
  }
  if (min_y) {
    *min_y = mn_y;
  }
  if (min_z) {
    *min_z = mn_z;
  }
  if (max_x) {
    *max_x = mx_x;
  }
  if (max_y) {
    *max_y = mx_y;
  }
  if (max_z) {
    *max_z = mx_z;
  }
}

}  // namespace detail
}  // namespace vista
