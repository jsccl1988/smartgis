// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_carto.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "gis/datasource/provider/impl/ogr/text/ogr_text_encoding.h"

namespace content {
namespace {

bool ascii_icontains(const char* hay, const char* needle) {
  if (!hay || !needle || !needle[0]) {
    return false;
  }
  const size_t n = std::strlen(needle);
  for (const char* p = hay; *p; ++p) {
    size_t i = 0;
    for (; i < n; ++i) {
      unsigned char a = static_cast<unsigned char>(p[i]);
      unsigned char b = static_cast<unsigned char>(needle[i]);
      if (a == 0) {
        return false;
      }
      if (a >= 'A' && a <= 'Z') {
        a = static_cast<unsigned char>(a - 'A' + 'a');
      }
      if (b >= 'A' && b <= 'Z') {
        b = static_cast<unsigned char>(b - 'A' + 'a');
      }
      if (a != b) {
        break;
      }
    }
    if (i == n) {
      return true;
    }
  }
  return false;
}

}  // namespace

COLORREF map_scene_map_bg_color() {
  // Baidu-like soft ocean behind land.
  return RGB(170, 211, 223);
}

COLORREF map_scene_river_color() {
  return RGB(100, 160, 208);
}

COLORREF map_scene_road_color() {
  // Distinct from river blue: Baidu-like highway gold.
  return RGB(196, 164, 106);
}

bool map_scene_label_boxes_overlap(MapLabelBox a, MapLabelBox b) {
  return a.left < b.right && b.left < a.right && a.top < b.bottom &&
         b.top < a.bottom;
}

size_t map_scene_accept_label_count(const MapLabelBox* boxes, size_t count) {
  if (!boxes || count == 0) {
    return 0;
  }
  std::vector<MapLabelBox> accepted;
  accepted.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    bool hit = false;
    for (const MapLabelBox& prev : accepted) {
      if (map_scene_label_boxes_overlap(prev, boxes[i])) {
        hit = true;
        break;
      }
    }
    if (!hit) {
      accepted.push_back(boxes[i]);
    }
  }
  return accepted.size();
}

int map_scene_label_min_importance(double scale) {
  // fit_extent(China) lands near scale 8–16. Allow prefecture / capital
  // stems (importance 2) so FlyCube MapFrame labels match leftover GDI
  // city names; keep counties (1) for closer zooms.
  if (scale < 22.0) {
    return 2;
  }
  if (scale < 48.0) {
    return 1;
  }
  return 0;
}

int map_scene_place_name_importance(const char* utf8_name) {
  if (!utf8_name || !utf8_name[0]) {
    return 0;
  }
  const std::wstring w = gis::datasource::ogr_bytes_to_wide(utf8_name);
  if (w.empty()) {
    return 0;
  }
  auto ends_with = [&w](std::wstring_view suffix) {
    return w.size() >= suffix.size() &&
           w.compare(w.size() - suffix.size(), suffix.size(), suffix) == 0;
  };
  // Wide literals use \u escapes so MSVC source-charset quirks cannot
  // inject U+FFFD into L"..." placename suffixes.
  if (ends_with(L"\u7279\u522b\u884c\u653f\u533a") ||
      ends_with(L"\u81ea\u6cbb\u533a") || ends_with(L"\u7701")) {
    return 3;
  }
  std::wstring stem = w;
  auto strip = [&stem](std::wstring_view suffix) {
    if (stem.size() > suffix.size() &&
        stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) ==
            0) {
      stem.resize(stem.size() - suffix.size());
    }
  };
  strip(L"\u7279\u522b\u884c\u653f\u533a");
  strip(L"\u81ea\u6cbb\u533a");
  strip(L"\u5e02");
  strip(L"\u7701");
  static constexpr const wchar_t* kCapitals[] = {
      L"\u5317\u4eac", L"\u5929\u6d25", L"\u4e0a\u6d77", L"\u91cd\u5e86",
      L"\u77f3\u5bb6\u5e84", L"\u592a\u539f", L"\u547c\u548c\u6d69\u7279",
      L"\u6c88\u9633", L"\u957f\u6625", L"\u54c8\u5c14\u6ee8", L"\u5357\u4eac",
      L"\u676d\u5dde", L"\u5408\u80a5", L"\u798f\u5dde", L"\u5357\u660c",
      L"\u6d4e\u5357", L"\u90d1\u5dde", L"\u6b66\u6c49", L"\u957f\u6c99",
      L"\u5e7f\u5dde", L"\u5357\u5b81", L"\u6d77\u53e3", L"\u6210\u90fd",
      L"\u8d35\u9633", L"\u6606\u660e",       L"\u62c9\u8428", L"\u897f\u5b89",
      L"\u5170\u5dde", L"\u897f\u5b81", L"\u94f6\u5ddd", L"\u5305\u5934",
      L"\u4e4c\u9c81\u6728\u9f50", L"\u9999\u6e2f", L"\u6fb3\u95e8",
      L"\u53f0\u5317",
  };
  for (const wchar_t* cap : kCapitals) {
    if (stem == cap) {
      return 3;
    }
  }
  if (ends_with(L"\u81ea\u6cbb\u5dde") || ends_with(L"\u5730\u533a") ||
      ends_with(L"\u76df") || ends_with(L"\u5dde") || ends_with(L"\u5e02")) {
    return 2;
  }
  if (ends_with(L"\u53bf") || ends_with(L"\u533a") || ends_with(L"\u65d7") ||
      ends_with(L"\u9547") || ends_with(L"\u4e61")) {
    return 1;
  }
  return 0;
}

MapLineRole map_scene_line_role(const char* kind, const char* feature_class) {
  auto generic = [](const char* s) {
    return !s || !s[0] || std::strcmp(s, "line") == 0 ||
           std::strcmp(s, "corridor") == 0;
  };
  auto water = [](const char* s) {
    return ascii_icontains(s, "river") || ascii_icontains(s, "water") ||
           ascii_icontains(s, "lake") || ascii_icontains(s, "stream") ||
           ascii_icontains(s, "canal");
  };
  auto road = [](const char* s) {
    return ascii_icontains(s, "road") || ascii_icontains(s, "highway") ||
           ascii_icontains(s, "street") || ascii_icontains(s, "motorway") ||
           ascii_icontains(s, "trunk");
  };
  if (!generic(kind) && water(kind)) {
    return MapLineRole::kWater;
  }
  if (!generic(feature_class) && water(feature_class)) {
    return MapLineRole::kWater;
  }
  if (!generic(kind) && road(kind)) {
    return MapLineRole::kRoad;
  }
  if (!generic(feature_class) && road(feature_class)) {
    return MapLineRole::kRoad;
  }
  return MapLineRole::kOther;
}

// china_city line layer mixes Natural Earth rivers (kind=river|lake) and
// trunk roads (kind=road, class=motorway|trunk|primary). Road styling fires
// when kind/class/highway/line_kind look like roads.

bool map_scene_line_is_major_class(const char* kind,
                                  const char* feature_class) {
  auto major = [](const char* s) {
    return ascii_icontains(s, "motorway") || ascii_icontains(s, "trunk") ||
           ascii_icontains(s, "highway") || ascii_icontains(s, "primary") ||
           ascii_icontains(s, "secondary") || ascii_icontains(s, "national") ||
           ascii_icontains(s, "express") ||
           // UTF-8 "高速" / "国道" as escapes (avoid MSVC source U+FFFD).
           ascii_icontains(s, "\xe9\xab\x98\xe9\x80\x9f") ||
           ascii_icontains(s, "\xe5\x9b\xbd\xe9\x81\x93");
  };
  return major(kind) || major(feature_class);
}

bool map_scene_line_visible_at_scale(MapLineRole role, double length,
                                    bool major_class, double scale) {
  // MapLibre style_align.json gates road/road-casing at minzoom 5 under Web
  // Mercator; product equirectangular china overview sits near scale 8–16
  // (zoom_from_scale ≈ 11). Show major arterials at national frame so dual
  // stroke casing is scoreable; keep a length floor so ramps stay culled.
  // Stem-aggregated length (batches) joins short trunk pieces; floor is the
  // bare-piece fallback (0.12° ≈ 13 km) so continuous gold roads read as
  // networks rather than sparse fragments.
  if (role == MapLineRole::kRoad && scale < 22.0) {
    return major_class && length >= 0.12;
  }
  if (major_class) {
    // Classed arterials still need a minimum run so ramps do not fill
    // regional frames.
    if (scale < 48.0) {
      return length >= 0.6;
    }
    return length >= 0.05 || scale >= 96.0;
  }
  double min_len = 0.0;
  if (role == MapLineRole::kWater) {
    if (scale < 22.0) {
      // Country frame: prefer stem-aggregated length (batches); bare pieces
      // still need a modest run so foreign NE stubs stay culled.
      min_len = 0.8;
    } else if (scale < 48.0) {
      min_len = 0.4;
    } else if (scale < 96.0) {
      min_len = 0.15;
    }
  } else if (role == MapLineRole::kRoad) {
    if (scale < 48.0) {
      min_len = 0.7;
    } else if (scale < 96.0) {
      min_len = 0.15;
    }
  }
  return length + 1e-9 >= min_len;
}

int map_scene_line_stroke_px(MapLineRole role, double length, double scale) {
  const bool trunk = length >= (role == MapLineRole::kRoad ? 3.0 : 4.0);
  if (role == MapLineRole::kWater || role == MapLineRole::kRoad) {
    if (scale < 22.0) {
      return trunk ? 2 : 1;
    }
    if (scale < 96.0) {
      return trunk ? 3 : 2;
    }
    return trunk ? 4 : 2;
  }
  return scale < 48.0 ? 1 : 2;
}

void map_scene_fill_stem_lengths(const MapStemSpan* spans, size_t count,
                                 double touch_tol, double* out_lengths) {
  if (!out_lengths) {
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    out_lengths[i] = 0.0;
  }
  if (!spans || count == 0) {
    return;
  }
  if (touch_tol < 0.0) {
    touch_tol = 0.0;
  }
  std::vector<size_t> parent(count);
  for (size_t i = 0; i < count; ++i) {
    parent[i] = i;
  }
  auto find_root = [&parent](size_t i) {
    while (parent[i] != i) {
      parent[i] = parent[parent[i]];
      i = parent[i];
    }
    return i;
  };
  auto unite = [&find_root, &parent](size_t a, size_t b) {
    a = find_root(a);
    b = find_root(b);
    if (a != b) {
      parent[b] = a;
    }
  };
  auto named = [](const char* s) { return s && s[0]; };
  const double tol2 = touch_tol * touch_tol;
  auto endpoints_close = [tol2](double ax, double ay, double bx, double by) {
    const double dx = ax - bx;
    const double dy = ay - by;
    return dx * dx + dy * dy <= tol2;
  };
  // Same-name joins in O(n) via first-index map; unnamed / touch still O(n^2).
  std::map<std::string_view, size_t> named_root;
  for (size_t i = 0; i < count; ++i) {
    if (!named(spans[i].name)) {
      continue;
    }
    const std::string_view key(spans[i].name);
    auto it = named_root.find(key);
    if (it == named_root.end()) {
      named_root.emplace(key, i);
    } else {
      unite(it->second, i);
    }
  }
  for (size_t i = 0; i < count; ++i) {
    for (size_t j = i + 1; j < count; ++j) {
      const bool both_named = named(spans[i].name) && named(spans[j].name);
      if (both_named) {
        // Same name already united; different names must not merge on touch.
        continue;
      }
      const bool touch =
          endpoints_close(spans[i].x0, spans[i].y0, spans[j].x0, spans[j].y0) ||
          endpoints_close(spans[i].x0, spans[i].y0, spans[j].x1, spans[j].y1) ||
          endpoints_close(spans[i].x1, spans[i].y1, spans[j].x0, spans[j].y0) ||
          endpoints_close(spans[i].x1, spans[i].y1, spans[j].x1, spans[j].y1);
      if (touch) {
        unite(i, j);
      }
    }
  }
  std::vector<double> root_sum(count, 0.0);
  for (size_t i = 0; i < count; ++i) {
    root_sum[find_root(i)] += spans[i].length;
  }
  for (size_t i = 0; i < count; ++i) {
    out_lengths[i] = root_sum[find_root(i)];
  }
}

double map_scene_stem_length(const MapStemSpan* spans, size_t count,
                             size_t index, double touch_tol) {
  if (!spans || index >= count) {
    return 0.0;
  }
  std::vector<double> all(count, 0.0);
  map_scene_fill_stem_lengths(spans, count, touch_tol, all.data());
  return all[index];
}

MapLineLabelAnchor map_scene_line_label_anchor(const double* xs, const double* ys,
                                              size_t count) {
  MapLineLabelAnchor out;
  if (!xs || !ys || count < 2) {
    return out;
  }
  double total = 0.0;
  for (size_t i = 1; i < count; ++i) {
    total += std::hypot(xs[i] - xs[i - 1], ys[i] - ys[i - 1]);
  }
  const double half = total * 0.5;
  double acc = 0.0;
  for (size_t i = 1; i < count; ++i) {
    const double dx = xs[i] - xs[i - 1];
    const double dy = ys[i] - ys[i - 1];
    const double seg = std::hypot(dx, dy);
    if (acc + seg + 1e-12 < half && i + 1 < count) {
      acc += seg;
      continue;
    }
    const double t =
        seg > 1e-12 ? std::clamp((half - acc) / seg, 0.0, 1.0) : 0.0;
    out.x = xs[i - 1] + t * dx;
    out.y = ys[i - 1] + t * dy;
    double deg = std::atan2(dy, dx) * (180.0 / 3.14159265358979323846);
    if (deg > 90.0) {
      deg -= 180.0;
    }
    if (deg <= -90.0) {
      deg += 180.0;
    }
    out.angle_deg = deg;
    out.ok = true;
    return out;
  }
  return out;
}

bool map_scene_extent_is_lonlat(double minx, double miny, double maxx,
                               double maxy) {
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

double map_scene_length_as_degrees(double length, bool lonlat) {
  if (lonlat) {
    return length;
  }
  constexpr double kMetersPerDegree = 111320.0;
  return length / kMetersPerDegree;
}

COLORREF map_scene_admin_stroke_color() {
  return RGB(196, 190, 176);
}

COLORREF map_scene_point_fill_color() {
  return RGB(90, 110, 130);
}

COLORREF map_scene_area_fill_color(const char* adcode, uint32_t feature_id) {
  // Unified Baidu land wash — boundaries carry identity, not choropleth fills.
  (void)adcode;
  (void)feature_id;
  return RGB(245, 243, 233);
}

}  // namespace content
