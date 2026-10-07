// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Batch policy for MapIR layout: line role, stem length, place-name
// rank, and scale visibility. Callers pass POD features, not GisScene.

#ifndef VISTA_COMPONENT_MAP_CARTO_FILTER_H_
#define VISTA_COMPONENT_MAP_CARTO_FILTER_H_

#include <cstddef>
#include <cstdint>

#include "vista/vista_export.h"

namespace vista {
namespace detail {

struct LabelScreenBox {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

VISTA_EXPORT bool label_boxes_overlap(LabelScreenBox a, LabelScreenBox b);
VISTA_EXPORT size_t accept_label_count(const LabelScreenBox* boxes, size_t count);

// Country fit on China is about scale 8–16. 3 = province/capital, 2 = city,
// 1 = county or river label, 0 = dense POI.
VISTA_EXPORT int label_min_importance(double scale);
VISTA_EXPORT int place_name_importance(const char* utf8_name);

enum class LineRole { kWater, kRoad, kOther };

VISTA_EXPORT LineRole line_role(const char* kind, const char* feature_class);
VISTA_EXPORT bool line_is_major_class(const char* kind, const char* feature_class);
VISTA_EXPORT bool line_visible_at_scale(LineRole role, double length,
                                      bool major_class, double scale);
VISTA_EXPORT int line_stroke_px(LineRole role, double length, double scale);

struct StemSpan {
  const char* name = nullptr;
  double length = 0;
  double x0 = 0;
  double y0 = 0;
  double x1 = 0;
  double y1 = 0;
};

VISTA_EXPORT double stem_length(const StemSpan* spans, size_t count, size_t index,
                              double touch_tol);
VISTA_EXPORT void fill_stem_lengths(const StemSpan* spans, size_t count,
                                  double touch_tol, double* out_lengths);

struct LineLabelAnchor {
  double x = 0;
  double y = 0;
  double angle_deg = 0;
  bool ok = false;
};

VISTA_EXPORT LineLabelAnchor line_label_anchor(const double* xs, const double* ys,
                                             size_t count);

VISTA_EXPORT bool extent_is_lonlat(double minx, double miny, double maxx,
                                 double maxy);
VISTA_EXPORT double length_as_degrees(double length, bool lonlat);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_CARTO_FILTER_H_
