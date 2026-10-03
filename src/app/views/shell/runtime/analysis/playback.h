// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_ANALYSIS_PLAYBACK_H_
#define APP_VIEWS_SHELL_RUNTIME_ANALYSIS_PLAYBACK_H_

#include <cstdint>
#include <string>
#include <vector>

#include "app/views/shell/runtime/analysis/flood_store.h"
#include "app/views/shell/runtime/analysis/orthogrid_store.h"
#include "app/views/shell/runtime/analysis/traffic_store.h"

namespace app {

// Which analysis product last filled the shared playback buffer.
enum class AnalysisProduct : std::uint8_t {
  kNone = 0,
  kTraffic,
  kFlood,
  kStormSurge,
  kOrthogrid,
  kOrthogrid3d,
};

// Shell-owned result buffer for CommitLayer + ResultPlayback + frame export.
// Composes product stores; plugins do not own this type — Browser writers fill
// it after processing.
class AnalysisPlayback {
 public:
  void reset();

  AnalysisProduct product() const { return product_; }
  int frame_count() const { return frame_count_; }
  int frame_index() const { return frame_index_; }
  double fps() const { return fps_; }
  bool looping() const { return looping_; }
  bool playing() const { return playing_; }

  void set_fps(double fps) { fps_ = fps > 0.0 ? fps : 12.0; }
  void set_looping(bool on) { looping_ = on; }
  void set_playing(bool on) { playing_ = on; }

  // Clamps to [0, frame_count-1] when frame_count > 0. Returns false if empty.
  bool set_frame_index(int index);

  const std::string& output_path() const { return output_path_; }
  const std::string& input_path() const { return input_path_; }
  void set_paths(std::string input, std::string output);

  // --- traffic ---
  void begin_traffic(std::vector<double> xy_interleaved,
                     double total_cost,
                     int frames,
                     std::string network_path);
  const std::vector<double>& traffic_xy() const { return traffic_.xy(); }
  double traffic_cost() const { return traffic_.cost(); }
  const std::string& network_path() const { return traffic_.network_path(); }
  int traffic_prefix_point_count() const;

  // --- flood / stormsurge ---
  void begin_flood(int width,
                   int height,
                   const double geotransform[6],
                   double water_level,
                   int expected_frames);
  void begin_stormsurge(int width,
                        int height,
                        const double geotransform[6],
                        double water_level,
                        int expected_frames);
  void push_flood_mask(const unsigned char* mask,
                       int width,
                       int height,
                       double water_level);
  void push_stormsurge_water_mesh(const double* xyz,
                                  int point_count,
                                  const int* triangles,
                                  int triangle_count,
                                  int frame_index);
  bool flood_ready() const;
  bool stormsurge_ready() const;
  int flood_width() const { return flood_.width(); }
  int flood_height() const { return flood_.height(); }
  double flood_water_level() const { return flood_.water_level(); }
  double flood_water_level_at(int frame) const {
    return flood_.water_level_at(frame);
  }
  const double* flood_geotransform() const { return flood_.geotransform(); }
  const std::vector<unsigned char>* flood_mask_at(int frame) const {
    return flood_.mask_at(frame);
  }
  const std::vector<double>* stormsurge_water_xyz_at(int frame) const {
    return flood_.water_xyz_at(frame);
  }
  const std::vector<int>* stormsurge_water_indices_at(int frame) const {
    return flood_.water_indices_at(frame);
  }

  // --- orthogrid / 3d ---
  void begin_orthogrid(int frames);
  void begin_orthogrid3d(int frames);
  void push_orthogrid_frame(int nx,
                            int ny,
                            const double* xs,
                            const double* ys);
  int orthogrid_nx() const { return orthogrid_.nx(); }
  int orthogrid_ny() const { return orthogrid_.ny(); }
  const std::vector<double>* orthogrid_xs_at(int frame) const {
    return orthogrid_.xs_at(frame);
  }
  const std::vector<double>* orthogrid_ys_at(int frame) const {
    return orthogrid_.ys_at(frame);
  }

  std::string playback_json() const;

 private:
  AnalysisProduct product_ = AnalysisProduct::kNone;
  int frame_count_ = 0;
  int frame_index_ = 0;
  double fps_ = 12.0;
  bool looping_ = true;
  bool playing_ = false;
  std::string input_path_;
  std::string output_path_;

  TrafficStore traffic_;
  FloodStore flood_;
  OrthogridStore orthogrid_;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_ANALYSIS_PLAYBACK_H_
