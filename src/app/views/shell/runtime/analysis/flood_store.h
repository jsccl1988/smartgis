// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_ANALYSIS_FLOOD_STORE_H_
#define APP_VIEWS_SHELL_RUNTIME_ANALYSIS_FLOOD_STORE_H_

#include <vector>

namespace app {

// Flood / storm-surge mask frames plus optional free-surface water meshes.
class FloodStore {
 public:
  void clear();

  void begin(int width,
             int height,
             const double geotransform[6],
             double water_level,
             int expected_frames);

  void push_mask(const unsigned char* mask,
                 int width,
                 int height,
                 double water_level);

  void push_water_mesh(const double* xyz,
                       int point_count,
                       const int* triangles,
                       int triangle_count,
                       int frame_index);

  int width() const { return width_; }
  int height() const { return height_; }
  double water_level() const { return water_level_; }
  double water_level_at(int frame) const;
  const double* geotransform() const { return gt_; }
  int frame_count() const { return static_cast<int>(masks_.size()); }

  const std::vector<unsigned char>* mask_at(int frame) const;
  const std::vector<double>* water_xyz_at(int frame) const;
  const std::vector<int>* water_indices_at(int frame) const;

 private:
  int width_ = 0;
  int height_ = 0;
  double gt_[6] = {};
  double water_level_ = 0.0;
  std::vector<std::vector<unsigned char>> masks_;
  std::vector<double> water_levels_;
  std::vector<std::vector<double>> water_xyz_;
  std::vector<std::vector<int>> water_indices_;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_ANALYSIS_FLOOD_STORE_H_
