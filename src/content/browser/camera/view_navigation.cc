// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/camera/view_navigation.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "content/browser/camera/map_host_extent.h"

namespace content {
namespace {

constexpr double kExtentEpsilonScale = 1e-7;

bool near_coord(double a, double b) {
  const double scale = 1.0 + std::max(std::abs(a), std::abs(b));
  return std::abs(a - b) <= kExtentEpsilonScale * scale;
}

// Same predicate as map_scene_extent_is_lonlat. Kept local so camera does not
// link the presenter that owns the shared symbol.
bool extent_is_lonlat(double minx, double miny, double maxx, double maxy) {
  if (!(maxx > minx) || !(maxy > miny)) {
    return true;
  }
  if (minx < -180.0 || maxx > 180.0 || miny < -90.0 || maxy > 90.0) {
    return false;
  }
  if ((maxx - minx) > 360.0 || (maxy - miny) > 180.0) {
    return false;
  }
  return true;
}

}  // namespace

bool extents_equal(const content::Extent2& a, const content::Extent2& b) {
  return near_coord(a.xmin, b.xmin) && near_coord(a.ymin, b.ymin) &&
         near_coord(a.xmax, b.xmax) && near_coord(a.ymax, b.ymax);
}

std::string format_view_scale(const content::Extent2& extent,
                              int viewport_width_px) {
  if (viewport_width_px <= 0) {
    return "1:—";
  }
  const double width = extent.xmax - extent.xmin;
  if (!(width > 0.0)) {
    return "1:1";
  }
  double width_m = width;
  if (extent_is_lonlat(extent.xmin, extent.ymin, extent.xmax, extent.ymax)) {
    // std::cos takes radians. The spec's center latitude is degrees.
    const double center_lat = (extent.ymin + extent.ymax) * 0.5;
    const double cos_lat = std::cos(center_lat * std::numbers::pi / 180.0);
    const double meters_per_degree = 111320.0 * std::max(cos_lat, 0.01);
    width_m = width * meters_per_degree;
  }
  const double n =
      (width_m / static_cast<double>(viewport_width_px)) * (96.0 / 0.0254);
  long long rounded = std::llround(n);
  if (rounded < 1) {
    rounded = 1;
  }
  return "1:" + std::to_string(rounded);
}

void ViewNavigation::reset(const content::Extent2& extent) {
  history_.clear();
  history_.push_back(extent);
  index_ = 0;
  extent_ = extent;
  status_.clear();
}

bool ViewNavigation::commit(const content::Extent2& next) {
  if (history_.empty()) {
    reset(next);
    return false;
  }
  if (extents_equal(history_[index_], next)) {
    extent_ = history_[index_];
    return false;
  }
  if (index_ + 1 < history_.size()) {
    history_.resize(index_ + 1);
  }
  history_.push_back(next);
  index_ = history_.size() - 1;
  while (history_.size() > kExtentCap) {
    history_.erase(history_.begin());
    if (index_ > 0) {
      --index_;
    }
  }
  extent_ = history_[index_];
  status_.clear();
  return true;
}

bool ViewNavigation::previous() {
  if (history_.empty() || index_ == 0) {
    status_ = "No previous extent";
    return false;
  }
  --index_;
  extent_ = history_[index_];
  status_.clear();
  return true;
}

bool ViewNavigation::next() {
  if (history_.empty() || index_ + 1 >= history_.size()) {
    status_ = "No next extent";
    return false;
  }
  ++index_;
  extent_ = history_[index_];
  status_.clear();
  return true;
}

bool ViewNavigation::zoom_to(const content::Extent2* target,
                             const char* empty_status) {
  if (!target || !extent_nonempty(*target)) {
    status_ = empty_status ? empty_status : "";
    return false;
  }
  commit(*target);
  status_.clear();
  return true;
}

bool ViewNavigation::zoom_layer(const content::Extent2* target) {
  return zoom_to(target, "No active layer");
}

bool ViewNavigation::zoom_selection(const content::Extent2* target) {
  return zoom_to(target, "No selection");
}

void ViewNavigation::note_no_feature() {
  status_ = "No feature";
}

std::string ViewNavigation::unique_label(const std::string& base) const {
  auto taken = [this](const std::string& label) {
    for (const ViewBookmark& mark : bookmarks_) {
      if (mark.label == label) {
        return true;
      }
    }
    return false;
  };
  if (!taken(base)) {
    return base;
  }
  for (int n = 2;; ++n) {
    const std::string candidate = base + " (" + std::to_string(n) + ")";
    if (!taken(candidate)) {
      return candidate;
    }
  }
}

void ViewNavigation::add_bookmark(std::string label) {
  if (label.empty()) {
    ++bookmark_serial_;
    label = "Bookmark " + std::to_string(bookmark_serial_);
  }
  ViewBookmark mark;
  mark.label = unique_label(label);
  mark.extent = extent_;
  bookmarks_.push_back(std::move(mark));
  status_.clear();
}

bool ViewNavigation::go_bookmark(size_t index) {
  if (index >= bookmarks_.size()) {
    return false;
  }
  commit(bookmarks_[index].extent);
  status_.clear();
  return true;
}

std::string ViewNavigation::scale_text(int viewport_width_px) const {
  return format_view_scale(extent_, viewport_width_px);
}

}  // namespace content
