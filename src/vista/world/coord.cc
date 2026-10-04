// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/coord.h"

#include <algorithm>

#include "vista/terrain/dem/dem_frame.h"

namespace render {

void leftover_yup_to_gis(double x0, double elev0, double lat0, double x1,
                         double elev1, double lat1, double* min_x,
                         double* min_y, double* min_z, double* max_x,
                         double* max_y, double* max_z) {
  // Mesh / AABB X is -lon; recover geographic lon for World envelope.
  const double lon0 = vista::dem_x_to_lon(static_cast<float>(x0));
  const double lon1 = vista::dem_x_to_lon(static_cast<float>(x1));
  if (min_x) {
    *min_x = (std::min)(lon0, lon1);
  }
  if (max_x) {
    *max_x = (std::max)(lon0, lon1);
  }
  if (min_y) {
    *min_y = (std::min)(lat0, lat1);
  }
  if (max_y) {
    *max_y = (std::max)(lat0, lat1);
  }
  if (min_z) {
    *min_z = (std::min)(elev0, elev1);
  }
  if (max_z) {
    *max_z = (std::max)(elev0, elev1);
  }
}

void leftover_aabb_to_gis(const Aabb& aabb, double* min_x, double* min_y,
                          double* min_z, double* max_x, double* max_y,
                          double* max_z) {
  leftover_yup_to_gis(
      static_cast<double>(aabb.vcMin.x), static_cast<double>(aabb.vcMin.y),
      static_cast<double>(aabb.vcMin.z), static_cast<double>(aabb.vcMax.x),
      static_cast<double>(aabb.vcMax.y), static_cast<double>(aabb.vcMax.z),
      min_x, min_y, min_z, max_x, max_y, max_z);
}

vista::Node* attach_gis_aabb(vista::World* world, const char* name, double min_x,
                           double min_y, double min_z, double max_x,
                           double max_y, double max_z) {
  if (!world) {
    return nullptr;
  }
  return world->add_node(vista::NodeKind::kEmpty, name, min_x, min_y, min_z,
                         max_x, max_y, max_z);
}

}  // namespace render
