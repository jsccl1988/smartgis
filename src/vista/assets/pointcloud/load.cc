// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/assets/pointcloud/load.h"

#include <cstring>
#include <string>

#include "vista/assets/pointcloud/text_io.h"

namespace vista {
namespace {

bool ends_with_ci(const std::string& s, const char* suffix) {
  const size_t n = std::strlen(suffix);
  if (s.size() < n) {
    return false;
  }
  for (size_t i = 0; i < n; ++i) {
    char a = s[s.size() - n + i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z') {
      a = static_cast<char>(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = static_cast<char>(b - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool load_point_cloud(const char* path, const LasLoadOptions& options,
                      PointCloud* out) {
  if (!out) {
    return false;
  }
  out->clear();
  if (!path || !*path) {
    out->error = "empty_path";
    return false;
  }
  const std::string p(path);
  if (ends_with_ci(p, ".txt") || ends_with_ci(p, ".xyz") ||
      ends_with_ci(p, ".csv")) {
    return load_pointcloud_text(path, out);
  }
  if (ends_with_ci(p, ".las") || ends_with_ci(p, ".laz")) {
    return load_las_file(path, options, out);
  }
  // Unknown: try LAS then text.
  if (load_las_file(path, options, out)) {
    return true;
  }
  const std::string las_err = out->error;
  if (load_pointcloud_text(path, out)) {
    return true;
  }
  out->error = las_err.empty() ? out->error : las_err;
  return false;
}

bool load_point_cloud(const char* path, PointCloud* out) {
  return load_point_cloud(path, LasLoadOptions{}, out);
}

}  // namespace vista
