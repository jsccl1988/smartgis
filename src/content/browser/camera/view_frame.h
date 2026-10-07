// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAMERA_VIEW_FRAME_H_
#define CONTENT_BROWSER_CAMERA_VIEW_FRAME_H_

#include "content/browser/camera/gis_host_extent.h"

namespace content {

class GisScene;

// 2D pan and scale for a map viewport, plus lon/lat framing of that view.
class ViewFrame {
 public:
  void apply_pan(int dx_px, int dy_px);
  void apply_zoom_at(int view_x, int view_y, double factor);
  void apply_pinch(int view_x, int view_y, double scale);
  void apply_world_extent(const content::Extent2& e, int view_w, int view_h);
  void fit_extent(const GisScene& scene, int view_w, int view_h);
  content::Extent2 view_world_extent(int view_w, int view_h) const;
  void map_to_view(double mx, double my, int* vx, int* vy) const;
  void view_to_map(int vx, int vy, double* mx, double* my) const;
  double scale() const { return scale_; }
  double pan_x() const { return pan_x_; }
  double pan_y() const { return pan_y_; }

 private:
  double pan_x_ = 0;
  double pan_y_ = 0;
  double scale_ = 1;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAMERA_VIEW_FRAME_H_
