// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_VISTA_DEM_HEIGHT_FIELD_H_
#define SMT_LEGACY_GIS_VISTA_DEM_HEIGHT_FIELD_H_

#include <cstdint>
#include <string>
#include <vector>

#include "vista/vista_export.h"
#include "vista/world/terrain/dem/dem_raster.h"

namespace render {

// Closed lon/lat ring used to clip a rectangular DEM to a country outline.
// prepare_bbox() is required before any_ring_contains / mask_outside_rings so
// ocean cells skip full point-in-polygon against every prefecture ring.
// Layout matches vista::LonLatRing; leftover ABI keeps this type name.
struct LonLatRing {
  std::vector<double> x;
  std::vector<double> y;
  mutable double minx = 0;
  mutable double miny = 0;
  mutable double maxx = 0;
  mutable double maxy = 0;
  mutable bool has_bbox = false;
  bool empty() const { return x.size() < 3 || x.size() != y.size(); }
  void prepare_bbox() const {
    has_bbox = false;
    if (empty()) {
      return;
    }
    minx = maxx = x[0];
    miny = maxy = y[0];
    for (size_t i = 1; i < x.size(); ++i) {
      if (x[i] < minx) {
        minx = x[i];
      }
      if (x[i] > maxx) {
        maxx = x[i];
      }
      if (y[i] < miny) {
        miny = y[i];
      }
      if (y[i] > maxy) {
        maxy = y[i];
      }
    }
    has_bbox = true;
  }
  bool bbox_may_contain(double px, double py) const {
    if (!has_bbox) {
      prepare_bbox();
    }
    return has_bbox && px >= minx && px <= maxx && py >= miny && py <= maxy;
  }
};

VISTA_EXPORT bool point_in_lonlat_ring(double px, double py,
                                     const LonLatRing& ring);
VISTA_EXPORT bool any_ring_contains(double px, double py,
                                  const std::vector<LonLatRing>& rings);

// Thin leftover shell over vista::DemRaster (authority for load / sample /
// mask / mesh). Public ABI names stay DemHeightField for SmtTerrain /
// map_to_scene; no parallel height grid.
class VISTA_EXPORT DemHeightField {
 public:
  bool load_gdal_raster(const char* path);
  void fill_synthetic_china();
  void fit_vertical_exaggeration();

  // Keep cells whose lon/lat fall inside any land ring. Empty rings = no-op.
  void mask_outside_rings(const std::vector<LonLatRing>& rings);

  void set_vertical_exaggeration(float k) {
    raster_.set_vertical_exaggeration(k);
  }
  float vertical_exaggeration() const {
    return raster_.vertical_exaggeration();
  }

  bool empty() const { return raster_.empty(); }
  int cols() const { return raster_.cols(); }
  int rows() const { return raster_.rows(); }

  float sample(double x, double y) const;
  float sample_meters(double x, double y) const;
  // Leftover Y-up normals (X=-lon). Not on DemRaster — adapter only.
  void sample_normal(double x, double y, float* nx, float* ny, float* nz) const;

  float drape_lift() const;
  float min_meters() const { return raster_.min_meters(); }
  float max_meters() const { return raster_.max_meters(); }
  void envelope(double* minx, double* miny, double* maxx, double* maxy) const;

  // Coarse XYZ (leftover Y-up) + triangle indices + optional hypsometric RGB /
  // normals. Mesh triangles come from DemRaster::build_mesh.
  bool build_mesh(int max_edge, std::vector<float>* xyz,
                  std::vector<unsigned>* indices, std::vector<float>* rgb,
                  std::vector<float>* nrm) const;

  const vista::DemRaster& dem_raster() const { return raster_; }
  vista::DemRaster& dem_raster() { return raster_; }

 private:
  vista::DemRaster raster_;
};

VISTA_EXPORT std::string find_sample_dem_path();
VISTA_EXPORT std::string find_sample_imagery_path();
VISTA_EXPORT std::string find_sample_model_path();

// Lower score is drawn first (more important).
VISTA_EXPORT int label_priority_from_fields(const char* name, const char* kind,
                                          const char* cls, const char* adcode);

struct MapLabelBox {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  int priority = 0;
};

// Greedy screen-space collision. keep receives candidate indices in draw order.
VISTA_EXPORT int declutter_map_labels(const MapLabelBox* boxes, int count,
                                    int max_keep, int view_w, int view_h,
                                    std::vector<int>* keep);

}  // namespace render

#endif  // SMT_LEGACY_GIS_VISTA_DEM_HEIGHT_FIELD_H_
