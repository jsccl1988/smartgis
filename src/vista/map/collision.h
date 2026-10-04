// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// HDC-free label collision. Numeric thresholds match the 2D cartographic
// declutter (priority, LOD max, budget, halo px, label box, line midpoint).

#ifndef GIS_VISTA_FRAME_DETAIL_COLLISION_H_
#define GIS_VISTA_FRAME_DETAIL_COLLISION_H_

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "vista/vista_export.h"

namespace vista {
namespace detail {

// Screen-space occupancy box. Lower priority scores win.
struct LabelBox {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  int priority = 0;
};

// Midpoint pose of a line-following label. Angle stays upright.
struct LineLabelPose {
  int x = 0;
  int y = 0;
  float angle_deg = 0.f;
};

int utf8_units(const char* text);

// Bitmap-metric run width without a GlyphMetrics inject (CJK ≈ em, Latin ≈ 0.55em).
VISTA_EXPORT float estimate_run_width_px(const char* text, float px_h);

// Lower score wins (title / province before county).
int label_priority(const char* name, const char* kind, const char* cls,
                   const char* adcode);

// Max accepted priority at this scale (fblc = device px per world unit).
int lod_max_priority(float fblc);
int label_budget(float fblc);
// Former projected-label draw cap (ViewFrame scale / fblc).
VISTA_EXPORT int label_draw_budget(double scale);
// Eight point-label screen nudges. Index 0 is the unshifted anchor.
struct PointNudge {
  int dx = 0;
  int dy = 0;
};
VISTA_EXPORT int point_label_nudges(int text_h, PointNudge* out, int max_out);
int label_px(int priority, float fblc);
VISTA_EXPORT int halo_px(int priority);

LabelBox label_box(int x, int y, const char* text, int px_h, int priority);
LabelBox label_box_rotated(int x, int y, const char* text, int px_h,
                           int priority, float angle_deg);

// Axis-aligned bounds of an upright box after rotation about its center.
VISTA_EXPORT LabelBox rotate_label_box(const LabelBox& upright, float angle_deg);

// Grow the occupancy box so halos participate in declutter.
void expand_label_box_for_halo(LabelBox* box, float halo_width_px);

// Midpoint of polyline (fraction 0.5). Prefer line_label_pose_at for slots.
bool line_label_pose(const int* xy_pairs, int n_pts, LineLabelPose* out);

// Place at fraction of total path length in [0, 1].
VISTA_EXPORT bool line_label_pose_at(const int* xy_pairs, int n_pts,
                                   float fraction, LineLabelPose* out);

// Sum of segment lengths in screen px.
VISTA_EXPORT float line_path_length_px(const int* xy_pairs, int n_pts);

// Ordered along-line retry fractions (midpoint first). Returns count written.
VISTA_EXPORT int line_label_slot_fractions(int max_out, float* out_fractions);

// True when the path is long enough to host a text run of the given width.
VISTA_EXPORT bool line_fits_label(float path_len_px, float text_width_px);

bool boxes_overlap(const LabelBox& a, const LabelBox& b);

// Spatial hash of kept boxes for one frame.
class LabelGrid {
 public:
  // Inline so every caller compiles field offsets with this header.
  // An out-of-line reset in another TU has cleared the wrong members when
  // that TU was rebuilt and Layout::build was not.
  void reset(float fblc, int view_w, int view_h) {
    const float scale = fblc > 0.f ? fblc : 1.f;
    view_w_ = view_w > 0 ? view_w : 800;
    view_h_ = view_h > 0 ? view_h : 600;
    max_priority_ = lod_max_priority(scale);
    budget_ = (std::min)(label_budget(scale),
                         label_draw_budget(static_cast<double>(scale)));
    cell_size_ = 32;
    labels_.clear();
    grid_.clear();
  }
  bool try_keep(const LabelBox& box);

 private:
  bool box_in_view(const LabelBox& box) const;
  static int64_t cell_key(int cell_x, int cell_y);
  void insert(size_t index, const LabelBox& box, int cell_size);
  bool overlaps_kept(const LabelBox& box, int cell_size) const;

  int view_w_ = 800;
  int view_h_ = 600;
  int max_priority_ = 9;
  int budget_ = 200;
  // One cell size for the whole frame. Per-box sizes miss overlaps.
  int cell_size_ = 32;
  std::vector<LabelBox> labels_;
  std::unordered_map<int64_t, std::vector<size_t>> grid_;
};

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_FRAME_DETAIL_COLLISION_H_
