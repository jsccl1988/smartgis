// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_WORLD_DEM_RASTER_H_
#define GIS_WORLD_DEM_RASTER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "gis/gis_export.h"
#include "gis/world/dem_frame.h"
#include "gis/world/land_mask.h"
#include "gis/world/scene.h"

namespace gis {

// Regular-grid elevation in map CRS for World / GpuScene seeding.
// Mesh XYZ is leftover Y-up (X=-lon, elev, lat) so RH lookAt looking north
// places east on screen-right; World AABB keeps geographic lon/lat + elev in Z.
class GIS_EXPORT DemRaster {
 public:
  bool load_gdal_raster(const char* path);
  void fill_synthetic_china();
  void fit_vertical_exaggeration();

  void mask_outside_rings(const std::vector<LonLatRing>& rings);

  void set_vertical_exaggeration(float k) { vert_exag_ = k; }
  float vertical_exaggeration() const { return vert_exag_; }

  bool empty() const { return heights_.empty() || cols_ < 2 || rows_ < 2; }
  int cols() const { return cols_; }
  int rows() const { return rows_; }

  float sample(double x, double y) const;
  float sample_meters(double x, double y) const;
  float min_meters() const { return min_m_; }
  float max_meters() const { return max_m_; }
  void envelope(double* minx, double* miny, double* maxx, double* maxy) const;

  // Coarse XYZ (X=-lon, elev, lat) + triangle indices.
  bool build_mesh(int max_edge, std::vector<float>* xyz,
                  std::vector<uint32_t>* indices) const;

 private:
  int index_at(int col, int row) const { return row * cols_ + col; }
  float meters_at(int col, int row) const;
  void recompute_range();
  void downsample_to_max_edge(int max_edge);

  int cols_ = 0;
  int rows_ = 0;
  double minx_ = 0;
  double miny_ = 0;
  double maxx_ = 0;
  double maxy_ = 0;
  float min_m_ = 0;
  float max_m_ = 1;
  float vert_exag_ = 0.0012f;
  std::vector<float> heights_;
  std::vector<uint8_t> land_;
};

GIS_EXPORT std::string find_sample_dem_path();

// Attach kTerrain + coarse mesh from |dem| into |world|.
GIS_EXPORT Node* seed_dem_raster_into_world(World* world, const DemRaster& dem,
                                            const char* name,
                                            int max_edge = 96);

// Views / shell helper: load sample DEM (or synthetic China), optional land
// mask, seed World. Does not touch leftover DemHeightField.
GIS_EXPORT Node* seed_china_dem_into_world(
    World* world, const LonLatRing* rings, size_t ring_count, const char* name,
    int max_edge = 96);

}  // namespace gis

#endif  // GIS_WORLD_DEM_RASTER_H_
