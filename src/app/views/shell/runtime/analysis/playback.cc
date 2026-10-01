// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/analysis/playback.h"

#include <algorithm>
#include <sstream>

namespace app {

void AnalysisPlayback::reset() {
  product_ = AnalysisProduct::kNone;
  frame_count_ = 0;
  frame_index_ = 0;
  fps_ = 12.0;
  looping_ = true;
  playing_ = false;
  input_path_.clear();
  output_path_.clear();
  traffic_.clear();
  flood_.clear();
  orthogrid_.clear();
}

bool AnalysisPlayback::set_frame_index(int index) {
  if (frame_count_ <= 0) {
    return false;
  }
  frame_index_ = std::clamp(index, 0, frame_count_ - 1);
  return true;
}

void AnalysisPlayback::set_paths(std::string input, std::string output) {
  input_path_ = std::move(input);
  output_path_ = std::move(output);
}

void AnalysisPlayback::begin_traffic(std::vector<double> xy_interleaved,
                                     double total_cost,
                                     int frames,
                                     std::string network_path) {
  reset();
  product_ = AnalysisProduct::kTraffic;
  traffic_.begin(std::move(xy_interleaved), total_cost, frames,
                 std::move(network_path));
  frame_count_ = traffic_.frame_count();
  frame_index_ = frame_count_ > 0 ? frame_count_ - 1 : 0;
}

int AnalysisPlayback::traffic_prefix_point_count() const {
  return traffic_.prefix_point_count(frame_index_);
}

void AnalysisPlayback::begin_flood(int width,
                                   int height,
                                   const double geotransform[6],
                                   double water_level,
                                   int expected_frames) {
  reset();
  product_ = AnalysisProduct::kFlood;
  flood_.begin(width, height, geotransform, water_level, expected_frames);
  frame_count_ = 0;
  frame_index_ = 0;
}

void AnalysisPlayback::begin_stormsurge(int width,
                                        int height,
                                        const double geotransform[6],
                                        double water_level,
                                        int expected_frames) {
  begin_flood(width, height, geotransform, water_level, expected_frames);
  product_ = AnalysisProduct::kStormSurge;
}

void AnalysisPlayback::push_flood_mask(const unsigned char* mask,
                                       int width,
                                       int height,
                                       double water_level) {
  flood_.push_mask(mask, width, height, water_level);
  frame_count_ = flood_.frame_count();
  frame_index_ = frame_count_ > 0 ? frame_count_ - 1 : 0;
}

void AnalysisPlayback::push_stormsurge_water_mesh(const double* xyz,
                                                  int point_count,
                                                  const int* triangles,
                                                  int triangle_count,
                                                  int frame_index) {
  if (product_ != AnalysisProduct::kStormSurge) {
    return;
  }
  flood_.push_water_mesh(xyz, point_count, triangles, triangle_count,
                         frame_index);
}

bool AnalysisPlayback::flood_ready() const {
  return product_ == AnalysisProduct::kFlood && frame_count_ > 0 &&
         flood_.width() > 0 && flood_.height() > 0;
}

bool AnalysisPlayback::stormsurge_ready() const {
  return product_ == AnalysisProduct::kStormSurge && frame_count_ > 0 &&
         flood_.width() > 0 && flood_.height() > 0;
}

void AnalysisPlayback::begin_orthogrid(int frames) {
  reset();
  product_ = AnalysisProduct::kOrthogrid;
  orthogrid_.begin(frames);
  frame_count_ = 0;
  frame_index_ = 0;
}

void AnalysisPlayback::begin_orthogrid3d(int frames) {
  reset();
  product_ = AnalysisProduct::kOrthogrid3d;
  orthogrid_.begin(frames);
  frame_count_ = std::max(1, frames);
  frame_index_ = frame_count_ - 1;
}

void AnalysisPlayback::push_orthogrid_frame(int nx,
                                            int ny,
                                            const double* xs,
                                            const double* ys) {
  if (product_ != AnalysisProduct::kOrthogrid &&
      product_ != AnalysisProduct::kOrthogrid3d) {
    return;
  }
  orthogrid_.push_frame(nx, ny, xs, ys);
  frame_count_ = orthogrid_.frame_count();
  frame_index_ = frame_count_ > 0 ? frame_count_ - 1 : 0;
}

std::string AnalysisPlayback::playback_json() const {
  std::ostringstream o;
  o << "{\"product\":\"";
  switch (product_) {
    case AnalysisProduct::kTraffic:
      o << "traffic";
      break;
    case AnalysisProduct::kFlood:
      o << "flood";
      break;
    case AnalysisProduct::kStormSurge:
      o << "stormsurge";
      break;
    case AnalysisProduct::kOrthogrid:
      o << "orthogrid";
      break;
    case AnalysisProduct::kOrthogrid3d:
      o << "orthogrid3d";
      break;
    default:
      o << "none";
      break;
  }
  o << "\",\"frame_count\":" << frame_count_
    << ",\"frame_index\":" << frame_index_ << ",\"fps\":" << fps_
    << ",\"looping\":" << (looping_ ? "true" : "false");
  if (!input_path_.empty()) {
    o << ",\"input\":\"" << input_path_ << "\"";
  }
  if (!output_path_.empty()) {
    o << ",\"output\":\"" << output_path_ << "\"";
  }
  o << "}";
  return o.str();
}

}  // namespace app
