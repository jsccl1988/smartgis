// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_FIELD_STORE_H_
#define GIS_ATMOSPHERE_FIELD_STORE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gis/atmosphere/field_channel.h"
#include "gis/gis_export.h"

namespace gis {
namespace atmosphere {

// Lon/lat rectangular regular grid for one field layer.
struct FieldGrid {
  double min_lon = 0.0;
  double min_lat = 0.0;
  double max_lon = 0.0;
  double max_lat = 0.0;
  int cols = 0;
  int rows = 0;

  bool empty() const { return cols < 1 || rows < 1; }
  std::size_t cell_count() const {
    return empty() ? 0
                   : static_cast<std::size_t>(cols) *
                         static_cast<std::size_t>(rows);
  }
};

// One channel layer: External or Procedural, mixed by priority + valid_mask.
struct FieldLayer {
  FieldChannel channel = FieldChannel::kWindU;
  FieldSourceKind kind = FieldSourceKind::kProcedural;
  int priority = 0;
  FieldGrid grid;
  // Row-major values; size == grid.cell_count() when populated.
  std::vector<float> values;
  // 0 = invalid (skip in mix); empty means all valid.
  std::vector<uint8_t> valid_mask;
  // Optional time stamp for temporal lerp (seconds); NaN = timeless.
  double time_sec = 0.0;
};

// Unique shared field plane for ocean / cloud systems and GPU upload.
class GIS_EXPORT FieldStore {
 public:
  FieldStore();
  ~FieldStore();

  FieldStore(const FieldStore&) = delete;
  FieldStore& operator=(const FieldStore&) = delete;

  // Replace or insert a layer for its channel (keyed by kind + priority).
  void set_layer(const FieldLayer& layer);

  // Upload one time-slice into the (channel, kind, priority) group.
  // Multiple distinct time_sec values accumulate for temporal lerp; set_layer
  // replaces the whole group. values size must equal cols*rows; valid_mask
  // may be empty or same size.
  bool upload_slice(FieldChannel channel, FieldSourceKind kind, int priority,
                    const FieldGrid& grid, const float* values,
                    std::size_t value_count, const uint8_t* valid_mask,
                    std::size_t mask_count, double time_sec);

  // Spatially bilinear (or nearest) sample with clamp; temporal lerp when
  // multiple slices exist. Returns 0 when no contributing layer.
  float sample(FieldChannel channel, double lon, double lat,
               double time_sec) const;

  std::size_t layer_count() const { return layers_.size(); }
  const FieldLayer* layer_at(std::size_t index) const;

 private:
  std::vector<FieldLayer> layers_;
};

}  // namespace atmosphere
}  // namespace gis

#endif  // GIS_ATMOSPHERE_FIELD_STORE_H_
