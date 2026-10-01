// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_IDW_H_
#define GIS_ANALYSIS_GEOCHEM_IDW_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/analysis/geochem/samples.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

// IDW interpolation surface for one element (row-major float grid).
// |values| cover the padded sample bbox (full-extent heat raster).
struct GeochemIdwResult {
  bool ok = false;
  int width = 0;
  int height = 0;
  double geotransform[6] = {};
  std::vector<float> values;
  // 1 = anomaly (value >= threshold), 0 otherwise.
  std::vector<unsigned char> anomaly_mask;
  double threshold = 0;
  std::string element;
  std::string error;
};

// Inverse-distance weighting on an axis-aligned grid covering sample extent
// (padded). |power| default 2; |cell_count| is max(width,height) target (≥8).
// When |threshold| is NaN, uses mean + k_sigma * stddev of sample values.
GIS_EXPORT GeochemIdwResult run_geochem_idw(const GeochemSampleSet& set,
                                            std::string_view element,
                                            int cell_count,
                                            double power,
                                            double threshold,
                                            double k_sigma);

// Write Float32 GeoTIFF of IDW values; optional Byte anomaly mask path.
GIS_EXPORT bool write_geochem_idw_geotiff(std::string_view output_path,
                                          const GeochemIdwResult& result,
                                          std::string_view mask_path);

// JSON: input, element, output; optional cells, power, threshold, k_sigma,
// mask_output.
GIS_EXPORT bool run_geochem_idw_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_IDW_H_
