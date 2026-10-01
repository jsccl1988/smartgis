// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_WORLD_DEM_RASTER_H_
#define GIS_WORLD_DEM_RASTER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "gis/gis_export.h"
#include "gis/vista/world/terrain/dem/dem_frame.h"
#include "gis/vista/world/terrain/process/land_mask.h"
#include "gis/vista/world/world.h"

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
  // Optional |uvs|: 2 floats per vertex (DEM col/row → 0..1) so land-only
  // meshes drape the full DEM texture correctly (AABB UV samples ocean).
  bool build_mesh(int max_edge, std::vector<float>* xyz,
                  std::vector<uint32_t>* indices) const;
  bool build_mesh(int max_edge, std::vector<float>* xyz,
                  std::vector<uint32_t>* indices,
                  std::vector<float>* uvs) const;

  // Mesh only DEM cells overlapping the lon/lat window (intersected with the
  // DEM envelope). Used by view-tiling so present does not upload one full
  // China coarse mesh every frame. When |apply_land_mask| is false, ocean
  // cells stay as verts (rectangular regional skirt; no chewed coast edge).
  bool build_mesh_window(double minx, double miny, double maxx, double maxy,
                         int max_edge, std::vector<float>* xyz,
                         std::vector<uint32_t>* indices,
                         std::vector<float>* uvs = nullptr,
                         bool apply_land_mask = true) const;

  // Discrete LOD buckets from camera distance (orbit / map units).
  // Near → denser mesh (larger max_edge). Clamped to [min_edge, max_edge_cap].
  // Default cap is high enough that the China showcase (distance ~2.55) and
  // overview (~3.2) stay dense enough to avoid stair-step coasts.
  static int lod_max_edge(float camera_distance, int min_edge = 32,
                          int max_edge_cap = 288);

  // Vertex count estimate for a given max_edge (for LOD tests / UI).
  static int lod_expected_vertices(int cols, int rows, int max_edge);

  // Bake terrain albedo RGBA8 sized to the DEM grid (downsampled with
  // |max_edge|). Lowlands stay green for landish gates; steep faces shift
  // toward rock and high flats toward snow. Ocean cells stay deep navy.
  bool bake_hypsometric_rgba(int max_edge, std::vector<uint8_t>* rgba,
                             int* out_w, int* out_h) const;

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
// Optional China remote-sensing / orthophoto beside the exe or under
// testing/data (china_rs.tif / china_imagery.tif). Empty when missing.
GIS_EXPORT std::string find_sample_imagery_path();

// Leftover SmartGis.exe hypsometric character (meters → RGB).
GIS_EXPORT void hypsometric_rgb(float meters, float* r, float* g, float* b);

// Load GDAL RGB(A) raster to tightly packed RGBA8. Logs to stderr on failure.
GIS_EXPORT bool load_imagery_rgba(const char* path, std::vector<uint8_t>* rgba,
                                  int* out_w, int* out_h);

// Attach kTerrain + coarse mesh from |dem| into |world|.
GIS_EXPORT Node* seed_dem_raster_into_world(World* world, const DemRaster& dem,
                                            const char* name,
                                            int max_edge = 96);

// Same as seed_dem_raster_into_world but picks max_edge from camera distance.
GIS_EXPORT Node* seed_dem_raster_lod_into_world(World* world,
                                                const DemRaster& dem,
                                                const char* name,
                                                float camera_distance);

// Seed one or more kTerrain tiles covering the lon/lat view AABB. Closer
// camera → more tiles / denser max_edge. Total vertices stay under
// |max_total_vertices|. Returns the number of terrain nodes attached.
// Documented interactive budget: 65536 verts (china overview stays well under).
GIS_EXPORT size_t seed_dem_view_tiles_into_world(
    World* world, const DemRaster& dem, double view_minx, double view_miny,
    double view_maxx, double view_maxy, float camera_distance,
    int max_total_vertices = 65536, const char* name_prefix = "dem_tile");

// Views / shell helper: load sample DEM (or synthetic China), optional land
// mask, seed World. Does not touch leftover DemHeightField.
GIS_EXPORT Node* seed_china_dem_into_world(
    World* world, const LonLatRing* rings, size_t ring_count, const char* name,
    int max_edge = 96);

}  // namespace gis

#endif  // GIS_WORLD_DEM_RASTER_H_
