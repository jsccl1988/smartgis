// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/analysis/flood_store.h"

#include <algorithm>

namespace app {

void FloodStore::clear() {
  width_ = 0;
  height_ = 0;
  for (double& v : gt_) {
    v = 0.0;
  }
  water_level_ = 0.0;
  masks_.clear();
  water_levels_.clear();
  water_xyz_.clear();
  water_indices_.clear();
}

void FloodStore::begin(int width,
                       int height,
                       const double geotransform[6],
                       double water_level,
                       int expected_frames) {
  clear();
  width_ = width;
  height_ = height;
  if (geotransform) {
    for (int i = 0; i < 6; ++i) {
      gt_[i] = geotransform[i];
    }
  }
  water_level_ = water_level;
  masks_.reserve(static_cast<size_t>(std::max(1, expected_frames)));
  water_levels_.reserve(static_cast<size_t>(std::max(1, expected_frames)));
}

void FloodStore::push_mask(const unsigned char* mask,
                           int width,
                           int height,
                           double water_level) {
  if (!mask || width <= 0 || height <= 0) {
    return;
  }
  // First push after a soft begin(0,0) / mismatched DEM must still accept.
  if (width_ <= 0 || height_ <= 0 || width != width_ || height != height_) {
    begin(width, height, gt_, water_level, 1);
  }
  const size_t n =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  masks_.emplace_back(mask, mask + n);
  water_levels_.push_back(water_level);
  water_level_ = water_level;
}

void FloodStore::push_water_mesh(const double* xyz,
                                 int point_count,
                                 const int* triangles,
                                 int triangle_count,
                                 int frame_index) {
  if (!xyz || point_count < 3 || !triangles || triangle_count < 1 ||
      frame_index < 0) {
    return;
  }
  const size_t need = static_cast<size_t>(frame_index) + 1u;
  if (water_xyz_.size() < need) {
    water_xyz_.resize(need);
    water_indices_.resize(need);
  }
  auto& dst_xyz = water_xyz_[static_cast<size_t>(frame_index)];
  auto& dst_idx = water_indices_[static_cast<size_t>(frame_index)];
  dst_xyz.assign(xyz, xyz + static_cast<size_t>(point_count) * 3u);
  dst_idx.assign(triangles,
                 triangles + static_cast<size_t>(triangle_count) * 3u);
}

double FloodStore::water_level_at(int frame) const {
  if (frame >= 0 && frame < static_cast<int>(water_levels_.size())) {
    return water_levels_[static_cast<size_t>(frame)];
  }
  return water_level_;
}

const std::vector<unsigned char>* FloodStore::mask_at(int frame) const {
  if (frame < 0 || frame >= static_cast<int>(masks_.size())) {
    return nullptr;
  }
  return &masks_[static_cast<size_t>(frame)];
}

const std::vector<double>* FloodStore::water_xyz_at(int frame) const {
  if (frame < 0 || frame >= static_cast<int>(water_xyz_.size())) {
    return nullptr;
  }
  const auto& xyz = water_xyz_[static_cast<size_t>(frame)];
  return xyz.size() >= 9 ? &xyz : nullptr;
}

const std::vector<int>* FloodStore::water_indices_at(int frame) const {
  if (frame < 0 || frame >= static_cast<int>(water_indices_.size())) {
    return nullptr;
  }
  const auto& idx = water_indices_[static_cast<size_t>(frame)];
  return idx.size() >= 3 ? &idx : nullptr;
}

}  // namespace app
