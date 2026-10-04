// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/pointcloud/ingest/text_io.h"

#include <cstdio>
#include <fstream>
#include <string>

namespace vista {

bool load_pointcloud_text(const char* path, PointCloud* out) {
  if (!out) {
    return false;
  }
  out->clear();
  if (!path || !*path) {
    out->error = "empty_path";
    return false;
  }
  out->source_path = path;
  std::ifstream in(path);
  if (!in) {
    out->error = "open_failed";
    return false;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    float x = 0;
    float z = 0;
    float y = 0;
    int r = 255;
    int g = 255;
    int b = 255;
    const int n =
        std::sscanf(line.c_str(), "%f,%f,%f,%d,%d,%d", &x, &z, &y, &r, &g, &b);
    if (n < 3) {
      // Also accept space-separated lon elev lat (Browser sample dialect).
      float lon = 0;
      float elev = 0;
      float lat = 0;
      if (std::sscanf(line.c_str(), "%f %f %f", &lon, &elev, &lat) == 3) {
        x = lon;
        y = lat;
        z = elev;
        r = g = b = 200;
      } else {
        continue;
      }
    }
    out->xyz.push_back(x);
    out->xyz.push_back(y);
    out->xyz.push_back(z);
    out->rgba.push_back(static_cast<uint8_t>(r < 0 ? 0 : (r > 255 ? 255 : r)));
    out->rgba.push_back(static_cast<uint8_t>(g < 0 ? 0 : (g > 255 ? 255 : g)));
    out->rgba.push_back(static_cast<uint8_t>(b < 0 ? 0 : (b > 255 ? 255 : b)));
    out->rgba.push_back(255);
  }
  if (out->empty()) {
    out->error = "no_points";
    return false;
  }
  out->recompute_bounds();
  return true;
}

}  // namespace vista
