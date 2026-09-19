// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_DEM_HEIGHT_FIELD_H_
#define SMT_LEGACY_RENDER_SCENE3D_DEM_HEIGHT_FIELD_H_

#include <cstdint>
#include <string>
#include <vector>

namespace gis {
class DemRaster;
}

#if defined(DEM_HEIGHT_FIELD_STATIC)
#define SCENE3D_EXPORT_DEFINED
#define SCENE3D_EXPORT_API
#define SCENE3D_EXPORT_CLASS
#endif

#if !defined(SCENE3D_EXPORT_DEFINED)
#define SCENE3D_EXPORT_DEFINED
#if defined(SCENE3D_EXPORTS)
#define SCENE3D_EXPORT_API __declspec(dllexport)
#define SCENE3D_EXPORT_CLASS __declspec(dllexport)
#else
#define SCENE3D_EXPORT_API __declspec(dllimport)
#define SCENE3D_EXPORT_CLASS __declspec(dllimport)
#endif
#endif

namespace render {

// Closed lon/lat ring used to clip a rectangular DEM to a country outline.
// prepare_bbox() is required before any_ring_contains / mask_outside_rings so
// ocean cells skip full point-in-polygon against every prefecture ring.
struct LonLatRing {
  std::vector<double> x;
  std::vector<double> y;
  mutable double minx = 0;
  mutable double miny = 0;
  mutable double maxx = 0;
  mutable double maxy = 0;
  mutable bool has_bbox = false;
  bool empty() const { return x.size() < 3 || x.size() != y.size(); }
  void prepare_bbox() const;
  bool bbox_may_contain(double px, double py) const;
};

SCENE3D_EXPORT_API bool point_in_lonlat_ring(double px, double py,
                                             const LonLatRing& ring);
SCENE3D_EXPORT_API bool any_ring_contains(double px, double py,
                                          const std::vector<LonLatRing>& rings);

// Regular-grid elevation in map CRS. sample() returns leftover-3D Y
// (geo X → 3D X, geo Y → 3D Z, height → 3D Y).
class SCENE3D_EXPORT_CLASS DemHeightField {
 public:
  bool load_gdal_raster(const char* path);
  void fill_synthetic_china();
  void fit_vertical_exaggeration();

  // Keep cells whose lon/lat fall inside any land ring. Empty rings = no-op.
  void mask_outside_rings(const std::vector<LonLatRing>& rings);

  void set_vertical_exaggeration(float k) { vert_exag_ = k; }
  float vertical_exaggeration() const { return vert_exag_; }

  bool empty() const { return heights_.empty() || cols_ < 2 || rows_ < 2; }
  int cols() const { return cols_; }
  int rows() const { return rows_; }

  float sample(double x, double y) const;
  float sample_meters(double x, double y) const;
  void sample_normal(double x, double y, float* nx, float* ny, float* nz) const;

  float drape_lift() const;
  float min_meters() const { return min_m_; }
  float max_meters() const { return max_m_; }
  void envelope(double* minx, double* miny, double* maxx, double* maxy) const;

  // Coarse XYZ (leftover Y-up) + triangle indices + hypsometric RGB.
  bool build_mesh(int max_edge, std::vector<float>* xyz,
                  std::vector<unsigned>* indices, std::vector<float>* rgb,
                  std::vector<float>* nrm) const;

 private:
  // Fill private grid from gis::DemRaster (defined in .cc; DemRaster is authority).
  bool assign_from_dem_raster(const gis::DemRaster& src);

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

SCENE3D_EXPORT_API std::string find_sample_dem_path();
SCENE3D_EXPORT_API std::string find_sample_imagery_path();
SCENE3D_EXPORT_API std::string find_sample_model_path();

// Lower score is drawn first (more important).
SCENE3D_EXPORT_API int label_priority_from_fields(const char* name,
                                                  const char* kind,
                                                  const char* cls,
                                                  const char* adcode);

struct MapLabelBox {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  int priority = 0;
};

// Greedy screen-space collision. keep receives candidate indices in draw order.
SCENE3D_EXPORT_API int declutter_map_labels(const MapLabelBox* boxes, int count,
                                            int max_keep, int view_w,
                                            int view_h, std::vector<int>* keep);

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_DEM_HEIGHT_FIELD_H_
