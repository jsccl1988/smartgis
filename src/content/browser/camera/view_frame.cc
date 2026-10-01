// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/camera/view_frame.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "content/browser/document/map_scene.h"
#include "tool/nav/camera_nav.h"

namespace content {

void ViewFrame::apply_pan(int dx_px, int dy_px) {
  pan_x_ += static_cast<double>(dx_px);
  pan_y_ += static_cast<double>(dy_px);
}

void ViewFrame::apply_zoom_at(int view_x, int view_y, double factor) {
  tool::zoom_at_client_point(&pan_x_, &pan_y_, &scale_, view_x, view_y, factor);
}

void ViewFrame::apply_pinch(int view_x, int view_y, double scale) {
  const int32_t wheel = tool::scale_to_wheel_delta(scale);
  if (wheel == 0) {
    return;
  }
  apply_zoom_at(view_x, view_y, tool::wheel_zoom_factor(wheel));
}

content::Extent2 ViewFrame::view_world_extent(int view_w, int view_h) const {
  if (view_w <= 0) {
    view_w = 800;
  }
  if (view_h <= 0) {
    view_h = 600;
  }
  double x0 = 0;
  double y0 = 0;
  double x1 = 0;
  double y1 = 0;
  view_to_map(0, 0, &x0, &y0);
  view_to_map(view_w, view_h, &x1, &y1);
  const double xmin = (std::min)(x0, x1);
  const double xmax = (std::max)(x0, x1);
  const double lat0 = -y0;
  const double lat1 = -y1;
  return content::Extent2{xmin, (std::min)(lat0, lat1), xmax,
                          (std::max)(lat0, lat1)};
}

void ViewFrame::apply_world_extent(const content::Extent2& e, int view_w,
                                   int view_h) {
  if (!extent_nonempty(e)) {
    return;
  }
  if (view_w <= 0) {
    view_w = 800;
  }
  if (view_h <= 0) {
    view_h = 600;
  }
  const double minx = e.xmin;
  const double maxx = e.xmax;
  const double miny = -e.ymax;
  const double maxy = -e.ymin;
  tool::WorldExtent box;
  box.minx = minx;
  box.maxx = maxx;
  box.miny = miny;
  box.maxy = maxy;
  tool::frame_world_extent(&pan_x_, &pan_y_, &scale_, box, view_w, view_h, 0.0);
}

void ViewFrame::fit_extent(const MapScene& scene, int view_w, int view_h) {
  if (view_w <= 0) {
    view_w = 800;
  }
  if (view_h <= 0) {
    view_h = 600;
  }
  // China prefecture packs: Natural Earth rivers that only touch the loose
  // China bbox keep foreign stubs (Siberia / Central Asia), and area layers
  // include South China Sea vertices near ~4N. Framing on all vertices zooms
  // out so rivers appear to "spill" past provincial land. Match maplibre_align
  // mainland envelope (equirectangular; Mercator remains a known gap).
  if (scene.has_china_extent()) {
    apply_world_extent(kChinaMap2dFrameExtent, view_w, view_h);
    return;
  }

  // Prefer land polygons when present so line/point outliers do not dominate.
  // Plugin product docs (traffic path, geochem points) are often line/point
  // only — fall back to full vertex envelope in map space.
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  if (!scene.polygon_fit_box(&minx, &miny, &maxx, &maxy)) {
    if (!scene.compute_extent(&minx, &miny, &maxx, &maxy)) {
      return;
    }
  }
  if (maxx - minx < 1.0) {
    const double c = 0.5 * (minx + maxx);
    minx = c - 0.5;
    maxx = c + 0.5;
  }
  if (maxy - miny < 1.0) {
    const double c = 0.5 * (miny + maxy);
    miny = c - 0.5;
    maxy = c + 0.5;
  }
  tool::WorldExtent box;
  box.minx = minx;
  box.miny = miny;
  box.maxx = maxx;
  box.maxy = maxy;
  tool::frame_world_extent(&pan_x_, &pan_y_, &scale_, box, view_w, view_h,
                           0.08);
}

void ViewFrame::map_to_view(double mx, double my, int* vx, int* vy) const {
  // Keep GDI points inside a safe 16-bit-ish range. Extreme pan/zoom or bad
  // vertices otherwise overflow int and can AV inside Polygon/LineTo.
  constexpr double kLo = -30000.0;
  constexpr double kHi = 30000.0;
  if (vx) {
    const double x = mx * scale_ + pan_x_;
    *vx = static_cast<int>(std::lround(x < kLo ? kLo : (x > kHi ? kHi : x)));
  }
  if (vy) {
    const double y = my * scale_ + pan_y_;
    *vy = static_cast<int>(std::lround(y < kLo ? kLo : (y > kHi ? kHi : y)));
  }
}

void ViewFrame::view_to_map(int vx, int vy, double* mx, double* my) const {
  if (mx) {
    *mx = (static_cast<double>(vx) - pan_x_) / scale_;
  }
  if (my) {
    *my = (static_cast<double>(vy) - pan_y_) / scale_;
  }
}

}  // namespace content
