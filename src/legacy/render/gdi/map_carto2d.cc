// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/gdi/map_carto2d.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace render {

int carto2d_utf8_units(const char* text) {
  if (!text || !text[0]) {
    return 1;
  }
  int n = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
       *p; ++p) {
    if ((*p & 0xc0) != 0x80) {
      ++n;
    }
  }
  return (std::max)(1, n);
}

int carto2d_label_priority(const char* name, const char* kind, const char* cls,
                           const char* adcode) {
  if (cls && std::strcmp(cls, "title") == 0) {
    return 0;
  }
  if (cls && std::strcmp(cls, "region_label") == 0) {
    return 1;
  }
  if (adcode && std::strlen(adcode) >= 6) {
    const char* digits = adcode;
    while (*digits && (*digits < '0' || *digits > '9')) {
      ++digits;
    }
    const size_t n = std::strlen(digits);
    if (n >= 6) {
      const char* tail4 = digits + n - 4;
      const char* tail2 = digits + n - 2;
      if (std::strcmp(tail4, "0000") == 0) {
        return 1;
      }
      if (std::strcmp(tail2, "00") == 0) {
        return 2;
      }
      return 6;
    }
  }
  if (cls && std::strcmp(cls, "river_label") == 0) {
    return 3;
  }
  if (kind && (std::strcmp(kind, "river") == 0 ||
               std::strcmp(kind, "water") == 0)) {
    return 3;
  }
  if (kind && carto2d_is_road_kind(kind)) {
    return 4;
  }
  if (name && *name) {
    const std::string n(name);
    if (n.find("省") != std::string::npos ||
        n.find("自治区") != std::string::npos ||
        n.find("特别行政区") != std::string::npos) {
      return 1;
    }
    if (n.find("自治州") != std::string::npos ||
        n.find("地区") != std::string::npos ||
        n.find("盟") != std::string::npos) {
      return 2;
    }
    if (n.find("县") != std::string::npos || n.find("旗") != std::string::npos) {
      return 6;
    }
    if (n.find("市") != std::string::npos) {
      return 2;
    }
  }
  if (kind &&
      (std::strcmp(kind, "city") == 0 || std::strcmp(kind, "point") == 0)) {
    return 2;
  }
  if (kind &&
      (std::strcmp(kind, "region") == 0 || std::strcmp(kind, "area") == 0)) {
    return 1;
  }
  return 5;
}

int carto2d_lod_max_priority(float fblc) {
  if (fblc < 12.f) {
    return 1;
  }
  if (fblc < 28.f) {
    return 2;
  }
  if (fblc < 60.f) {
    return 3;
  }
  return 9;
}

int carto2d_label_budget(float fblc) {
  if (fblc < 12.f) {
    return 28;
  }
  if (fblc < 28.f) {
    return 56;
  }
  if (fblc < 60.f) {
    return 96;
  }
  return 220;
}

int carto2d_label_px(int priority, float fblc) {
  if (priority <= 1) {
    return fblc < 12.f ? 15 : 17;
  }
  if (priority == 2) {
    return fblc < 18.f ? 14 : 16;
  }
  if (priority == 3) {
    return 13;
  }
  return 12;
}

int carto2d_halo_px(int priority) {
  return priority <= 2 ? 3 : 2;
}

int carto2d_point_radius(float fblc) {
  // Country / regional scale: thin Baidu-like POI discs (not fat GIS dots).
  if (fblc < 12.f) {
    return 1;
  }
  if (fblc < 28.f) {
    return 1;
  }
  if (fblc < 60.f) {
    return 2;
  }
  return 3;
}

int carto2d_stroke_px(float fblc, bool river) {
  return carto2d_stroke_px_kind(fblc, river, false);
}

int carto2d_stroke_px_kind(float fblc, bool river, bool road) {
  if (river) {
    if (fblc < 12.f) {
      return 2;
    }
    if (fblc < 28.f) {
      return 2;
    }
    if (fblc < 60.f) {
      return 3;
    }
    return 4;
  }
  if (road) {
    return fblc < 28.f ? 1 : 2;
  }
  if (fblc < 28.f) {
    return 1;
  }
  return 2;
}

unsigned carto2d_map_bg() {
  // Baidu-like soft ocean / empty canvas behind land polygons.
  return 0x00dfd3aa;  // RGB(170, 211, 223)
}

unsigned carto2d_land_fill() {
  // Baidu standard land: warm cream. Admin identity is on the stroke.
  return 0x00e9f3f5;  // RGB(245, 243, 233)
}

unsigned carto2d_water_fill() {
  return 0x00ffcca3;  // RGB(163, 204, 255)
}

unsigned carto2d_boost_fill(unsigned bgr) {
  (void)bgr;
  return carto2d_land_fill();
}

unsigned carto2d_admin_stroke() {
  return 0x00b0bec4;  // RGB(196, 190, 176)
}

unsigned carto2d_river_color() {
  // Clear but not neon; hydrography stays behind labels.
  return 0x00d0a064;  // RGB(100, 160, 208)
}

unsigned carto2d_road_color() {
  return 0x00ffffff;  // RGB(255, 255, 255)
}

bool carto2d_is_river_kind(const char* kind) {
  return kind && (std::strcmp(kind, "river") == 0 ||
                  std::strcmp(kind, "water") == 0);
}

bool carto2d_is_road_kind(const char* kind) {
  return kind && (std::strcmp(kind, "road") == 0 ||
                  std::strcmp(kind, "highway") == 0 ||
                  std::strcmp(kind, "expressway") == 0);
}

int carto2d_point_min_distance(float fblc) {
  if (fblc < 12.f) {
    return 30;
  }
  if (fblc < 28.f) {
    return 18;
  }
  if (fblc < 60.f) {
    return 8;
  }
  return 3;
}

MapCartoBox carto2d_label_box(int x, int y, const char* text, int px_h,
                              int priority) {
  const int h = (std::max)(12, px_h);
  const int w = carto2d_utf8_units(text) * (h * 3 / 5) + 8;
  MapCartoBox box;
  box.left = x - 2;
  box.top = y - 2;
  box.right = x + w;
  box.bottom = y + h + 2;
  box.priority = priority;
  return box;
}

bool carto2d_boxes_overlap(const MapCartoBox& a, const MapCartoBox& b) {
  return a.left < b.right && a.right > b.left && a.top < b.bottom &&
         a.bottom > b.top;
}

void MapCarto2dFrame::reset(float fblc, int view_w, int view_h) {
  fblc_ = fblc > 0.f ? fblc : 1.f;
  view_w_ = view_w > 0 ? view_w : 800;
  view_h_ = view_h > 0 ? view_h : 600;
  max_priority_ = carto2d_lod_max_priority(fblc_);
  budget_ = carto2d_label_budget(fblc_);
  min_dist_ = carto2d_point_min_distance(fblc_);
  labels_.clear();
  points_.clear();
}

bool MapCarto2dFrame::box_in_view(const MapCartoBox& box) const {
  return box.right > 4 && box.left < view_w_ - 4 && box.bottom > 4 &&
         box.top < view_h_ - 4;
}

bool MapCarto2dFrame::try_keep_label(const MapCartoBox& box) {
  if (box.priority > max_priority_) {
    return false;
  }
  if (static_cast<int>(labels_.size()) >= budget_) {
    return false;
  }
  if (!box_in_view(box)) {
    return false;
  }
  for (const MapCartoBox& kept : labels_) {
    if (carto2d_boxes_overlap(box, kept)) {
      return false;
    }
  }
  labels_.push_back(box);
  return true;
}

int MapCarto2dFrame::point_count() const {
  return static_cast<int>(points_.size() / 2);
}

bool MapCarto2dFrame::try_keep_point(int x, int y) {
  const int min_d2 = min_dist_ * min_dist_;
  for (size_t i = 0; i + 1 < points_.size(); i += 2) {
    const int dx = x - points_[i];
    const int dy = y - points_[i + 1];
    if (dx * dx + dy * dy < min_d2) {
      return false;
    }
  }
  points_.push_back(x);
  points_.push_back(y);
  return true;
}

}  // namespace render
