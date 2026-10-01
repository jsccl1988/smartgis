// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/gis/vista/dem_height_field.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "base/core/log.h"
#include "base/trace/event/process_trace.h"

namespace render {
namespace {

bool boxes_overlap(const MapLabelBox& a, const MapLabelBox& b) {
  return a.left < b.right && a.right > b.left && a.top < b.bottom &&
         a.bottom > b.top;
}

bool box_in_view(const MapLabelBox& b, int view_w, int view_h) {
  if (view_w <= 0 || view_h <= 0) {
    return true;
  }
  return b.right > 4 && b.left < view_w - 4 && b.bottom > 4 &&
         b.top < view_h - 4;
}

std::string join_dir(const std::string& dir, const char* rel) {
  return dir + rel;
}

std::vector<gis::LonLatRing> to_gis_rings(const std::vector<LonLatRing>& rings) {
  std::vector<gis::LonLatRing> out;
  out.reserve(rings.size());
  for (const LonLatRing& ring : rings) {
    gis::LonLatRing g;
    g.x = ring.x;
    g.y = ring.y;
    out.push_back(std::move(g));
  }
  return out;
}

}  // namespace

bool DemHeightField::load_gdal_raster(const char* path) {
  return raster_.load_gdal_raster(path);
}

void DemHeightField::fill_synthetic_china() {
  raster_.fill_synthetic_china();
}

void DemHeightField::fit_vertical_exaggeration() {
  raster_.fit_vertical_exaggeration();
}

void DemHeightField::mask_outside_rings(const std::vector<LonLatRing>& rings) {
  if (rings.empty() || raster_.empty()) {
    return;
  }
  raster_.mask_outside_rings(to_gis_rings(rings));
}

float DemHeightField::sample_meters(double x, double y) const {
  return raster_.sample_meters(x, y);
}

float DemHeightField::sample(double x, double y) const {
  return raster_.sample(x, y);
}

void DemHeightField::sample_normal(double x, double y, float* nx, float* ny,
                                   float* nz) const {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  raster_.envelope(&minx, &miny, &maxx, &maxy);
  const float span = static_cast<float>((std::max)(maxx - minx, maxy - miny));
  const float eps = (std::max)(0.08f, span / 180.f);
  const float hx0 = sample(x - eps, y);
  const float hx1 = sample(x + eps, y);
  const float hy0 = sample(x, y - eps);
  const float hy1 = sample(x, y + eps);
  // Leftover 3D: X=-lon (west+), Y up, Z north. Gradient ∂h/∂world_x flips
  // vs geographic east, so negate the X component of the cross product.
  const float dx = 2.f * eps;
  const float dz = 2.f * eps;
  float x_c = -(dz * (hx0 - hx1));
  float y_c = dx * dz;
  float z_c = dx * (hy0 - hy1);
  const float len = std::sqrt(x_c * x_c + y_c * y_c + z_c * z_c);
  if (len < 1e-8f) {
    x_c = 0.f;
    y_c = 1.f;
    z_c = 0.f;
  } else {
    x_c /= len;
    y_c /= len;
    z_c /= len;
  }
  if (nx) {
    *nx = x_c;
  }
  if (ny) {
    *ny = y_c;
  }
  if (nz) {
    *nz = z_c;
  }
}

float DemHeightField::drape_lift() const {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  raster_.envelope(&minx, &miny, &maxx, &maxy);
  const float span = static_cast<float>((std::max)(maxx - minx, maxy - miny));
  // Keep vectors clearly above the DEM to avoid z-fighting stripes.
  return span * 0.0014f + 0.08f;
}

void DemHeightField::envelope(double* minx, double* miny, double* maxx,
                              double* maxy) const {
  raster_.envelope(minx, miny, maxx, maxy);
}

bool DemHeightField::build_mesh(int max_edge, std::vector<float>* xyz,
                                std::vector<unsigned>* indices,
                                std::vector<float>* rgb,
                                std::vector<float>* nrm) const {
  BASE_TRACE_EVENT("build_mesh", "legacy.gis.vista");
  LOGGING(LOG_INFO, "[legacy.flow] vista.build_mesh");
  if (!xyz || !indices || raster_.empty()) {
    return false;
  }
  std::vector<uint32_t> idx32;
  if (!raster_.build_mesh(max_edge, xyz, &idx32) || xyz->empty() ||
      idx32.empty()) {
    return false;
  }
  indices->assign(idx32.begin(), idx32.end());
  if (rgb) {
    rgb->clear();
    rgb->reserve(xyz->size());
  }
  if (nrm) {
    nrm->clear();
    nrm->reserve(xyz->size());
  }
  if (!rgb && !nrm) {
    return true;
  }
  for (size_t i = 0; i + 2 < xyz->size(); i += 3) {
    const float wx = (*xyz)[i];
    const float lat = (*xyz)[i + 2];
    const double lon = static_cast<double>(-wx);
    const float meters = sample_meters(lon, static_cast<double>(lat));
    if (rgb) {
      float r = 0;
      float g = 0;
      float b = 0;
      gis::hypsometric_rgb(meters, &r, &g, &b);
      rgb->push_back(r);
      rgb->push_back(g);
      rgb->push_back(b);
    }
    if (nrm) {
      float nx = 0;
      float ny = 1;
      float nz = 0;
      sample_normal(lon, static_cast<double>(lat), &nx, &ny, &nz);
      nrm->push_back(nx);
      nrm->push_back(ny);
      nrm->push_back(nz);
    }
  }
  return true;
}

bool point_in_lonlat_ring(double px, double py, const LonLatRing& ring) {
  // Inline even-odd — do not copy ring.x/y into gis::LonLatRing per cell
  // (that O(cells×vertices) allocation hung 3D view open on china_city).
  if (ring.empty()) {
    return false;
  }
  const size_t n = ring.x.size();
  bool inside = false;
  size_t j = n - 1;
  for (size_t i = 0; i < n; ++i) {
    const double xi = ring.x[i];
    const double yi = ring.y[i];
    const double xj = ring.x[j];
    const double yj = ring.y[j];
    const bool hit = ((yi > py) != (yj > py)) &&
                     (px < (xj - xi) * (py - yi) / ((yj - yi) + 0.0) + xi);
    if (hit) {
      inside = !inside;
    }
    j = i;
  }
  return inside;
}

bool any_ring_contains(double px, double py,
                       const std::vector<LonLatRing>& rings) {
  for (const LonLatRing& ring : rings) {
    if (!ring.bbox_may_contain(px, py)) {
      continue;
    }
    if (point_in_lonlat_ring(px, py, ring)) {
      return true;
    }
  }
  return false;
}

std::string find_sample_dem_path() {
  // Single discovery path with gis to avoid leftover/gis drift.
  return gis::find_sample_dem_path();
}

namespace {

std::string first_existing_beside_exe(const char* const* rel, int count) {
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  std::string dir;
  if (n > 0 && n < MAX_PATH) {
    dir.assign(module, module + n);
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) {
      dir.resize(slash + 1);
    }
  }
  for (int i = 0; i < count; ++i) {
    const std::string cand = join_dir(dir, rel[i]);
    const DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return cand;
    }
  }
  return {};
}

}  // namespace

std::string find_sample_imagery_path() {
  const char* rel[] = {
      "china_rs.tif",
      "china_imagery.tif",
      "china_rs.tiff",
      "testing\\data\\china_rs.tif",
      "testing\\data\\china_imagery.tif",
      "..\\testing\\data\\china_rs.tif",
      "..\\..\\testing\\data\\china_rs.tif",
  };
  return first_existing_beside_exe(
      rel, static_cast<int>(sizeof(rel) / sizeof(rel[0])));
}

std::string find_sample_model_path() {
  const char* rel[] = {
      "china_model.glb",
      "china_model.gltf",
      "china_model.obj",
      "testing\\data\\china_model.glb",
      "testing\\data\\china_model.obj",
      "..\\testing\\data\\china_model.glb",
      "..\\..\\testing\\data\\china_model.glb",
  };
  return first_existing_beside_exe(
      rel, static_cast<int>(sizeof(rel) / sizeof(rel[0])));
}

int label_priority_from_fields(const char* name, const char* kind,
                               const char* cls, const char* adcode) {
  if (cls && std::strcmp(cls, "title") == 0) {
    return 0;
  }
  if (cls && std::strcmp(cls, "region_label") == 0) {
    return 1;
  }
  if (adcode && std::strlen(adcode) >= 6) {
    const char* tail = adcode + std::strlen(adcode) - 4;
    if (std::strcmp(tail, "0000") == 0) {
      return 1;
    }
    if (std::strcmp(tail, "0100") == 0) {
      return 2;
    }
    return 6;
  }
  if (kind &&
      (std::strcmp(kind, "region") == 0 || std::strcmp(kind, "area") == 0)) {
    return 1;
  }
  if (cls && std::strcmp(cls, "river_label") == 0) {
    return 3;
  }
  if (kind && std::strcmp(kind, "river") == 0) {
    return 3;
  }
  // Latin-only line hydro names must not compete with city points.
  if (kind && std::strcmp(kind, "line") == 0) {
    return 5;
  }
  if (kind &&
      (std::strcmp(kind, "city") == 0 || std::strcmp(kind, "point") == 0)) {
    return 2;
  }
  if (name && *name) {
    const std::string n(name);
    if (n.find("省") != std::string::npos ||
        n.find("自治区") != std::string::npos ||
        n.find("特别行政区") != std::string::npos) {
      return 1;
    }
    if (n.size() <= 9 && n.find("市") != std::string::npos) {
      return 2;
    }
  }
  return 5;
}

int declutter_map_labels(const MapLabelBox* boxes, int count, int max_keep,
                         int view_w, int view_h, std::vector<int>* keep) {
  if (!keep) {
    return 0;
  }
  keep->clear();
  if (!boxes || count <= 0 || max_keep <= 0) {
    return 0;
  }
  std::vector<int> order(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    order[static_cast<size_t>(i)] = i;
  }
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    if (boxes[a].priority != boxes[b].priority) {
      return boxes[a].priority < boxes[b].priority;
    }
    return a < b;
  });
  for (int idx : order) {
    const MapLabelBox& box = boxes[idx];
    if (!box_in_view(box, view_w, view_h)) {
      continue;
    }
    bool hit = false;
    for (int kept : *keep) {
      if (boxes_overlap(box, boxes[kept])) {
        hit = true;
        break;
      }
    }
    if (hit) {
      continue;
    }
    keep->push_back(idx);
    if (static_cast<int>(keep->size()) >= max_keep) {
      break;
    }
  }
  return static_cast<int>(keep->size());
}

}  // namespace render
