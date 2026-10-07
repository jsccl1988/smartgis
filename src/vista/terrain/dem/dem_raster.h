// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_DEM_RASTER_H_
#define VISTA_TERRAIN_DEM_DEM_RASTER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "vista/vista_export.h"
#include "vista/terrain/dem/dem_frame.h"
#include "vista/terrain/process/land_mask.h"

namespace vista {

// Regular-grid elevation in map CRS for World / WorldPass seeding.
// Mesh XYZ is leftover Y-up (X=-lon, elev, lat) so RH lookAt looking north
// places east on screen-right; World AABB keeps geographic lon/lat + elev in Z.
class VISTA_EXPORT DemRaster {
 public:
  bool load_gdal_raster(const char* path);
  // Deprecated stand-in (kept for ABI). Product / tests must load real
  // china_dem via find_sample_dem_path() — do not call this.
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
  // Clamped grid sample (no bilinear). shade_dem_rgba LOD uses bilinear
  // separately; this remains the integer-cell accessor.
  float meters_at(int col, int row) const;
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
  // Default cap tracks china_dem 1536×960 (AWS Mapzen z7): national framing
  // (distance ~2.55) and overview (~3.2) stay denser than the old 288 bake era.
  static int lod_max_edge(float camera_distance, int min_edge = 32,
                          int max_edge_cap = 512);

  // Vertex count estimate for a given max_edge (for LOD tests / UI).
  static int lod_expected_vertices(int cols, int rows, int max_edge);

  // Bake terrain albedo RGBA8 sized to the DEM grid (downsampled with
  // |max_edge|). Lowlands stay green for landish gates; steep faces shift
  // toward rock and high flats toward snow. Ocean cells stay deep navy.
  bool bake_hypsometric_rgba(int max_edge, std::vector<uint8_t>* rgba,
                             int* out_w, int* out_h) const;

  // Origin-style jet elevation surface + isoline overlay (same downsample
  // as bake_hypsometric_rgba). Ocean cells stay transparent (A=0).
  bool bake_elevation_overlay_rgba(int max_edge, bool surface, bool curves,
                                   std::vector<uint8_t>* rgba, int* out_w,
                                   int* out_h) const;

  // Globe height grid via sample_meters plus albedo. |global_grid| uses about
  // 1024×512; otherwise about 1024×640. |imagery_path| is tried first;
  // bake_hypsometric_rgba is the fallback (1024). |imagery_loaded| is optional.
  // Does not retain GDAL buffers in the outputs.
  bool sample_globe_surface(double minx, double miny, double maxx, double maxy,
                            bool global_grid, const char* imagery_path,
                            std::vector<float>* heights, int* cols, int* rows,
                            std::vector<uint8_t>* rgba, int* tex_w, int* tex_h,
                            bool* imagery_loaded = nullptr) const;

  // Source path set by load_gdal_raster (empty for synthetic).
  const std::string& source_path() const { return source_path_; }

  // Fill from process/disk DEM bake cache (already LOD'd + land-masked).
  bool adopt_bake_cache(int cols, int rows, double minx, double miny,
                        double maxx, double maxy, float min_m, float max_m,
                        float vert_exag, std::vector<float> heights,
                        std::vector<uint8_t> land, const char* path);

  // Export grid for dem_bake_cache (pointers valid until next mutate).
  void export_bake_cache(int* cols, int* rows, double* minx, double* miny,
                         double* maxx, double* maxy, float* min_m, float* max_m,
                         float* vert_exag, const std::vector<float>** heights,
                         const std::vector<uint8_t>** land) const;

 private:
  int index_at(int col, int row) const { return row * cols_ + col; }
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
  std::string source_path_;
};

// Default product DEM: china_dem (cutlined national outline). Does not prefer
// global_dem — that would remask China showcase with prefecture rings and punch
// plains holes. Use find_sample_global_dem_path / set_sample_dem_path_override
// for full-sphere / custom AOI.
VISTA_EXPORT std::string find_sample_dem_path();
// Prefer global_dem.tif (world3d / atmosphere globe); falls back to
// find_sample_dem_path (china stand-in) when global is missing.
VISTA_EXPORT std::string find_sample_global_dem_path();
// Optional product override for Scene3d terrain (global DEM / custom AOI).
// Empty clears the override so find_sample_dem_path falls back to china_dem.
VISTA_EXPORT void set_sample_dem_path_override(const char* path);
VISTA_EXPORT std::string sample_dem_path_override();
// Optional China remote-sensing / orthophoto beside the exe or under
// out/data (china_rs.tif / china_imagery.tif). Empty when missing.
VISTA_EXPORT std::string find_sample_imagery_path();
// Prefer global_terrain / blue_marble equirect; falls back to
// find_sample_imagery_path when global albedo is missing.
VISTA_EXPORT std::string find_sample_global_imagery_path();

// Elevation → RGB (atlas ramp). Lowlands stay green-dominant for landish BMP
// gates; highs stay ochre/taupe (not blown white).
VISTA_EXPORT void hypsometric_rgb(float meters, float* r, float* g, float* b);

// Lit PBR / stereo vertex albedo: hypsometric + rock on steep + snow on flats.
VISTA_EXPORT void terrain_material_rgb(float meters, float slope01, float* r,
                                     float* g, float* b);

// Load GDAL RGB(A) raster to tightly packed RGBA8. Logs to stderr on failure.
// |max_edge| caps the longer axis (GDAL RasterIO resamples). Globe albedo
// uses 2048; China orthophoto drape stays at 1024 so china_rs stays modest.
VISTA_EXPORT bool load_imagery_rgba(const char* path, std::vector<uint8_t>* rgba,
                                  int* out_w, int* out_h, int max_edge = 1024);

// Lon/lat box for world/dem_seed view tiles and china seed.
// Empty sample_dem_path_override forces the China box (73,18)-(135,54) when
// the input is empty or outside the China window. A set override keeps a
// non-empty input box (never forces China).
VISTA_EXPORT void dem_seed_lonlat_box(double in_minx, double in_miny,
                                    double in_maxx, double in_maxy,
                                    double* out_minx, double* out_miny,
                                    double* out_maxx, double* out_maxy);

// dem_view_seed_cache key: lod_max_edge(distance) * 10 + (distance < 2.4 ? 2 : 1).
VISTA_EXPORT int dem_seed_cache_key(float camera_distance);

}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_DEM_RASTER_H_
