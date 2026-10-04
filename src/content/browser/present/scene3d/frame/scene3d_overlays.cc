// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/scene3d_overlays.h"

#include <algorithm>
#include <cstdint>

#include "base/core/log.h"

namespace content {
namespace {

void remove_named_nodes(vista::World* world, vista::NodeKind kind,
                        const char* name) {
  if (!world || !name) {
    return;
  }
  for (size_t i = 0; i < world->node_count();) {
    const vista::Node* node = world->node_at(i);
    if (node && node->kind == kind && node->name == name) {
      if (!world->remove_node(node->id)) {
        ++i;
      }
      continue;
    }
    ++i;
  }
}

bool has_named_node(const vista::World* world, vista::NodeKind kind,
                    const char* name) {
  if (!world || !name) {
    return false;
  }
  for (size_t i = 0; i < world->node_count(); ++i) {
    const vista::Node* node = world->node_at(i);
    if (node && node->kind == kind && node->name == name) {
      return true;
    }
  }
  return false;
}

float dem_roof_y(const std::vector<float>& local_xyz, size_t dem_xyz_count) {
  float dem_max_y = -1.0e9f;
  for (size_t i = 1; i < dem_xyz_count && i < local_xyz.size(); i += 3) {
    dem_max_y = (std::max)(dem_max_y, local_xyz[i]);
  }
  if (!(dem_max_y > -1.0e8f)) {
    return 0.f;
  }
  return dem_max_y;
}

}  // namespace

void Scene3dOverlays::set_pointcloud(const float* xyz_lon_lat_elev,
                                     int point_count, const uint8_t* rgba) {
  xyz_geo_.clear();
  rgba_.clear();
  pointcloud_dirty_ = true;
  if (!xyz_lon_lat_elev || point_count < 1) {
    return;
  }
  const size_t n = static_cast<size_t>(point_count);
  xyz_geo_.assign(xyz_lon_lat_elev, xyz_lon_lat_elev + n * 3);
  if (rgba) {
    rgba_.assign(rgba, rgba + n * 4);
  }
}

void Scene3dOverlays::clear_pointcloud() {
  xyz_geo_.clear();
  rgba_.clear();
  pointcloud_dirty_ = true;
}

void Scene3dOverlays::set_tin(const float* xyz_lon_lat_elev, int point_count,
                              const unsigned* indices, int index_count,
                              const uint8_t* albedo_rgba) {
  tin_xyz_geo_.clear();
  tin_idx_.clear();
  tin_has_albedo_ = false;
  tin_dirty_ = true;
  if (!xyz_lon_lat_elev || point_count < 3 || !indices || index_count < 3) {
    return;
  }
  const size_t n = static_cast<size_t>(point_count);
  const size_t ic = static_cast<size_t>(index_count);
  if ((ic % 3u) != 0) {
    return;
  }
  tin_xyz_geo_.assign(xyz_lon_lat_elev, xyz_lon_lat_elev + n * 3);
  tin_idx_.assign(indices, indices + ic);
  if (albedo_rgba) {
    tin_albedo_[0] = albedo_rgba[0];
    tin_albedo_[1] = albedo_rgba[1];
    tin_albedo_[2] = albedo_rgba[2];
    tin_albedo_[3] = albedo_rgba[3];
  }
  tin_has_albedo_ = true;
}

void Scene3dOverlays::clear_tin() {
  tin_xyz_geo_.clear();
  tin_idx_.clear();
  tin_has_albedo_ = false;
  tin_dirty_ = true;
}

void Scene3dOverlays::attach_pointcloud(vista::World* world,
                                        const OrbitGeoFrame& geo,
                                        const std::vector<float>& local_xyz,
                                        size_t dem_xyz_count, bool force) {
  if (!world || xyz_geo_.empty() || !geo.valid) {
    return;
  }
  const size_t n = xyz_geo_.size() / 3;
  if (n == 0) {
    return;
  }
  if (!force && !pointcloud_dirty_ &&
      has_named_node(world, vista::NodeKind::kPointCloud,
                    "overlay_pointcloud")) {
    return;
  }
  remove_named_nodes(world, vista::NodeKind::kPointCloud, "overlay_pointcloud");
  std::vector<float> orbit_xyz;
  orbit_xyz.reserve(n * 3);
  float mn_x = 0.f;
  float mn_y = 0.f;
  float mn_z = 0.f;
  float mx_x = 0.f;
  float mx_y = 0.f;
  float mx_z = 0.f;
  for (size_t i = 0; i < n; ++i) {
    const double lon = static_cast<double>(xyz_geo_[i * 3]);
    const double lat = static_cast<double>(xyz_geo_[i * 3 + 1]);
    const float elev = xyz_geo_[i * 3 + 2];
    float ox = 0.f;
    float oy = 0.f;
    float oz = 0.f;
    geo.lon_lat_to_orbit(lon, lat, elev, &ox, &oy, &oz);
    orbit_xyz.push_back(ox);
    orbit_xyz.push_back(oy);
    orbit_xyz.push_back(oz);
    if (i == 0) {
      mn_x = mx_x = ox;
      mn_y = mx_y = oy;
      mn_z = mx_z = oz;
    } else {
      mn_x = (std::min)(mn_x, ox);
      mn_y = (std::min)(mn_y, oy);
      mn_z = (std::min)(mn_z, oz);
      mx_x = (std::max)(mx_x, ox);
      mx_y = (std::max)(mx_y, oy);
      mx_z = (std::max)(mx_z, oz);
    }
  }
  constexpr float kBeadBase = 0.32f;
  constexpr float kBeadSpan = 0.12f;
  const float geo_y0 = mn_y;
  const float geo_span = (std::max)(mx_y - mn_y, 1.0e-4f);
  const float target_base = dem_roof_y(local_xyz, dem_xyz_count) + kBeadBase;
  for (size_t i = 0; i < n; ++i) {
    const float t = (orbit_xyz[i * 3 + 1] - geo_y0) / geo_span;
    orbit_xyz[i * 3 + 1] = target_base + t * kBeadSpan;
  }
  mn_y = target_base;
  mx_y = target_base + kBeadSpan;
  vista::Node* node = world->attach_pointcloud("overlay_pointcloud", mn_x, mn_y,
                                               mn_z, mx_x, mx_y, mx_z);
  if (!node) {
    return;
  }
  const uint8_t* rgba = rgba_.size() == n * 4 ? rgba_.data() : nullptr;
  (void)world->set_pointcloud_points(node->id, orbit_xyz.data(), n, rgba,
                                     rgba ? n * 4 : 0);
  pointcloud_dirty_ = false;
}

void Scene3dOverlays::attach_tin(vista::World* world, const OrbitGeoFrame& geo,
                                 std::vector<float>* local_xyz,
                                 std::vector<unsigned>* local_idx,
                                 size_t dem_xyz_count, size_t dem_idx_count,
                                 bool force) {
  if (!world || !local_xyz || !local_idx || tin_xyz_geo_.empty() ||
      tin_idx_.empty() || !geo.valid) {
    return;
  }
  const size_t n = tin_xyz_geo_.size() / 3;
  if (n < 3) {
    return;
  }
  if (!force && !tin_dirty_ &&
      has_named_node(world, vista::NodeKind::kTerrain, "overlay_tin")) {
    return;
  }
  remove_named_nodes(world, vista::NodeKind::kTerrain, "overlay_tin");

  std::vector<float> orbit_xyz;
  orbit_xyz.reserve(n * 3);
  float mn_x = 0.f;
  float mn_y = 0.f;
  float mn_z = 0.f;
  float mx_x = 0.f;
  float mx_y = 0.f;
  float mx_z = 0.f;
  for (size_t i = 0; i < n; ++i) {
    const double lon = static_cast<double>(tin_xyz_geo_[i * 3]);
    const double lat = static_cast<double>(tin_xyz_geo_[i * 3 + 1]);
    const float elev = tin_xyz_geo_[i * 3 + 2];
    float ox = 0.f;
    float oy = 0.f;
    float oz = 0.f;
    geo.lon_lat_to_orbit(lon, lat, elev, &ox, &oy, &oz);
    orbit_xyz.push_back(ox);
    orbit_xyz.push_back(oy);
    orbit_xyz.push_back(oz);
    if (i == 0) {
      mn_x = mx_x = ox;
      mn_y = mx_y = oy;
      mn_z = mx_z = oz;
    } else {
      mn_x = (std::min)(mn_x, ox);
      mn_y = (std::min)(mn_y, oy);
      mn_z = (std::min)(mn_z, oz);
      mx_x = (std::max)(mx_x, ox);
      mx_y = (std::max)(mx_y, oy);
      mx_z = (std::max)(mx_z, oz);
    }
  }

  std::vector<uint32_t> orbit_idx;
  orbit_idx.reserve(tin_idx_.size());
  for (size_t t = 0; t + 2 < tin_idx_.size(); t += 3) {
    const unsigned a = tin_idx_[t];
    const unsigned b = tin_idx_[t + 1];
    const unsigned c = tin_idx_[t + 2];
    if (a >= n || b >= n || c >= n) {
      continue;
    }
    orbit_idx.push_back(static_cast<uint32_t>(a));
    orbit_idx.push_back(static_cast<uint32_t>(c));
    orbit_idx.push_back(static_cast<uint32_t>(b));
  }
  if (orbit_idx.size() < 3) {
    return;
  }

  const float geo_y0 = mn_y;
  const float geo_span = (std::max)(mx_y - mn_y, 1.0e-4f);
  constexpr float kOrbitClearance = 0.35f;
  constexpr float kOrbitSlabMin = 0.25f;
  constexpr float kOrbitSlabMax = 1.75f;
  const bool hex_albedo =
      tin_has_albedo_ && tin_albedo_[0] >= 160 && tin_albedo_[1] >= 100 &&
      tin_albedo_[2] < 140 && tin_albedo_[0] > tin_albedo_[2] + 40;
  const float slab_max = hex_albedo ? 2.35f : kOrbitSlabMax;
  const float slab_scale = hex_albedo ? 0.85f : 0.55f;
  const float orbit_slab =
      (std::min)(slab_max, (std::max)(kOrbitSlabMin, geo_span * slab_scale));
  const float dem_max_y = dem_roof_y(*local_xyz, dem_xyz_count);
  const float target_base = dem_max_y + kOrbitClearance;
  for (size_t i = 0; i < n; ++i) {
    const float t = (orbit_xyz[i * 3 + 1] - geo_y0) / geo_span;
    orbit_xyz[i * 3 + 1] = target_base + t * orbit_slab;
  }
  mn_y = target_base;
  mx_y = target_base + orbit_slab;
  LOGGING(LOG_INFO,
          "scene3d.present overlay_tin orbit_y=[%.3f,%.3f] geo_y0=%.3f "
          "dem_roof=%.3f verts=%zu idx=%zu",
          mn_y, mx_y, geo_y0, dem_max_y, n, orbit_idx.size());

  if (local_xyz->size() > dem_xyz_count || local_idx->size() > dem_idx_count) {
    local_xyz->resize(dem_xyz_count);
    local_idx->resize(dem_idx_count);
  }
  const size_t base_vert = local_xyz->size() / 3;
  constexpr float kPadXz = 0.12f;
  vista::Node* node = world->attach_terrain("overlay_tin", mn_x - kPadXz, mn_y,
                                            mn_z - kPadXz, mx_x + kPadXz, mx_y,
                                            mx_z + kPadXz);
  if (node) {
    (void)world->set_terrain_mesh(node->id, orbit_xyz.data(), orbit_xyz.size(),
                                  orbit_idx.data(), orbit_idx.size());
  }
  local_xyz->insert(local_xyz->end(), orbit_xyz.begin(), orbit_xyz.end());
  for (uint32_t vi : orbit_idx) {
    local_idx->push_back(
        static_cast<unsigned>(base_vert + static_cast<size_t>(vi)));
  }
  remove_named_nodes(world, vista::NodeKind::kPointCloud,
                     "overlay_tin_markers");
  tin_dirty_ = false;
}

}  // namespace content
