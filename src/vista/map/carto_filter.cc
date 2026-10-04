// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/carto_filter.h"

#include "vista/map/frame.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gis/datasource/ogr/ogr_text_encoding.h"
#include "ogrsf_frmts.h"

namespace vista {
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

namespace detail {

bool label_boxes_overlap(LabelScreenBox a, LabelScreenBox b) {
  return a.left < b.right && b.left < a.right && a.top < b.bottom &&
         b.top < a.bottom;
}

size_t accept_label_count(const LabelScreenBox* boxes, size_t count) {
  if (!boxes || count == 0) {
    return 0;
  }
  std::vector<LabelScreenBox> accepted;
  accepted.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    bool hit = false;
    for (const LabelScreenBox& prev : accepted) {
      if (label_boxes_overlap(prev, boxes[i])) {
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

int label_min_importance(double scale) {
  // fit_extent(China) lands near scale 8–16. Allow prefecture / capital
  // stems (importance 2) so MapFrame labels match city names; keep counties
  // (1) for closer zooms.
  if (scale < 22.0) {
    return 2;
  }
  if (scale < 48.0) {
    return 1;
  }
  return 0;
}

int place_name_importance(const char* utf8_name) {
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

LineRole line_role(const char* kind, const char* feature_class) {
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
    return LineRole::kWater;
  }
  if (!generic(feature_class) && water(feature_class)) {
    return LineRole::kWater;
  }
  if (!generic(kind) && road(kind)) {
    return LineRole::kRoad;
  }
  if (!generic(feature_class) && road(feature_class)) {
    return LineRole::kRoad;
  }
  return LineRole::kOther;
}

bool line_is_major_class(const char* kind, const char* feature_class) {
  auto major = [](const char* s) {
    return ascii_icontains(s, "motorway") || ascii_icontains(s, "trunk") ||
           ascii_icontains(s, "highway") || ascii_icontains(s, "primary") ||
           ascii_icontains(s, "secondary") || ascii_icontains(s, "national") ||
           ascii_icontains(s, "express") ||
           ascii_icontains(s, "\xe9\xab\x98\xe9\x80\x9f") ||
           ascii_icontains(s, "\xe5\x9b\xbd\xe9\x81\x93");
  };
  return major(kind) || major(feature_class);
}

bool line_visible_at_scale(LineRole role, double length, bool major_class,
                           double scale) {
  // MapLibre-style road gate at national frame (product scale ≈ 8–16).
  // Stem-aggregated length joins short trunk pieces; 0.12° is the bare-piece
  // floor so continuous roads read as networks.
  if (role == LineRole::kRoad && scale < 22.0) {
    return major_class && length >= 0.12;
  }
  if (major_class) {
    if (scale < 48.0) {
      return length >= 0.6;
    }
    return length >= 0.05 || scale >= 96.0;
  }
  double min_len = 0.0;
  if (role == LineRole::kWater) {
    if (scale < 22.0) {
      min_len = 0.8;
    } else if (scale < 48.0) {
      min_len = 0.4;
    } else if (scale < 96.0) {
      min_len = 0.15;
    }
  } else if (role == LineRole::kRoad) {
    if (scale < 48.0) {
      min_len = 0.7;
    } else if (scale < 96.0) {
      min_len = 0.15;
    }
  }
  return length + 1e-9 >= min_len;
}

int line_stroke_px(LineRole role, double length, double scale) {
  const bool trunk = length >= (role == LineRole::kRoad ? 3.0 : 4.0);
  if (role == LineRole::kWater || role == LineRole::kRoad) {
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

void fill_stem_lengths(const StemSpan* spans, size_t count, double touch_tol,
                       double* out_lengths) {
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

double stem_length(const StemSpan* spans, size_t count, size_t index,
                   double touch_tol) {
  if (!spans || index >= count) {
    return 0.0;
  }
  std::vector<double> all(count, 0.0);
  fill_stem_lengths(spans, count, touch_tol, all.data());
  return all[index];
}

LineLabelAnchor line_label_anchor(const double* xs, const double* ys,
                                  size_t count) {
  LineLabelAnchor out;
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

double length_as_degrees(double length, bool lonlat) {
  if (lonlat) {
    return length;
  }
  constexpr double kMetersPerDegree = 111320.0;
  return length / kMetersPerDegree;
}

}  // namespace detail

namespace {

bool is_carto_slot(const std::string& name) {
  return name == "land" || name == "water" || name == "river" ||
         name == "admin" || name == "road" || name == "label";
}

std::string ascii_lower(std::string text) {
  for (char& c : text) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return text;
}

struct FeatureFields {
  const char* kind = nullptr;
  const char* cls = nullptr;
  const char* fclass = nullptr;
  const char* anno = nullptr;
  const char* name = nullptr;
  const char* adcode = nullptr;
  const char* type = nullptr;
};

FeatureFields probe_fields(const BatchFeature& f) {
  FeatureFields out;
  for (const BatchField& field : f.fields) {
    if (!out.kind && field.name == "kind") {
      out.kind = field.value.c_str();
    } else if (!out.cls && field.name == "class") {
      out.cls = field.value.c_str();
    } else if (!out.fclass && field.name == "fclass") {
      out.fclass = field.value.c_str();
    } else if (!out.anno && field.name == "anno") {
      out.anno = field.value.c_str();
    } else if (!out.name && field.name == "name") {
      out.name = field.value.c_str();
    } else if (!out.adcode && field.name == "adcode") {
      out.adcode = field.value.c_str();
    } else if (!out.type && field.name == "type") {
      out.type = field.value.c_str();
    }
  }
  return out;
}

const char* feature_class_cstr(const FeatureFields& fields) {
  if (fields.cls && fields.cls[0]) {
    return fields.cls;
  }
  if (fields.fclass && fields.fclass[0]) {
    return fields.fclass;
  }
  return nullptr;
}

std::string feature_display_name(const FeatureFields& fields) {
  if (fields.anno && fields.anno[0]) {
    return fields.anno;
  }
  if (fields.name && fields.name[0]) {
    return fields.name;
  }
  return {};
}

int feature_label_importance(const FeatureFields& fields) {
  if (fields.cls) {
    if (std::strcmp(fields.cls, "title") == 0) {
      return 3;
    }
    if (std::strcmp(fields.cls, "region_label") == 0) {
      return 2;
    }
    if (std::strcmp(fields.cls, "river_label") == 0) {
      return 1;
    }
  }
  const std::string name = feature_display_name(fields);
  const int by_name = detail::place_name_importance(name.c_str());
  if (by_name >= 3) {
    return 3;
  }
  if (fields.adcode) {
    const size_t n = std::strlen(fields.adcode);
    if (n >= 6) {
      const bool z45 = fields.adcode[4] == '0' && fields.adcode[5] == '0';
      const bool z23 = fields.adcode[2] == '0' && fields.adcode[3] == '0';
      if (z45 && z23) {
        return 3;
      }
      if (z45) {
        return 2;
      }
      if (by_name > 0) {
        return by_name;
      }
      return 1;
    }
  }
  if (by_name > 0) {
    return by_name;
  }
  if (fields.kind) {
    if (std::strcmp(fields.kind, "city") == 0 ||
        std::strcmp(fields.kind, "point") == 0) {
      return 2;
    }
  }
  return 0;
}

std::string carto_source_layer(const BatchFeature& feature,
                               const FeatureFields& fields) {
  std::string kind;
  if (fields.kind && fields.kind[0]) {
    kind = ascii_lower(fields.kind);
  } else if (fields.type && fields.type[0]) {
    kind = ascii_lower(fields.type);
  } else if (fields.cls && fields.cls[0]) {
    kind = ascii_lower(fields.cls);
  } else if (fields.fclass && fields.fclass[0]) {
    kind = ascii_lower(fields.fclass);
  }
  const auto has = [&](const char* token) {
    return kind.find(token) != std::string::npos;
  };
  if (feature.kind == BatchGeomKind::kPolygon) {
    if (has("water") || has("lake") || has("sea") || has("ocean")) {
      return "water";
    }
    return "land";
  }
  if (feature.kind == BatchGeomKind::kLine) {
    if (has("river") || has("stream") || has("canal") || has("lake")) {
      return "river";
    }
    if (has("admin") || has("bound") || has("border")) {
      return "admin";
    }
    if (has("road") || has("highway") || has("motorway") || has("trunk") ||
        has("primary") || has("secondary") || has("street")) {
      return "road";
    }
    return "admin";
  }
  return "label";
}

LayerBatch* batch_for(LayerBatchSet* out, const std::string& source) {
  for (LayerBatch& batch : out->batches) {
    if (batch.source_layer == source) {
      return &batch;
    }
  }
  LayerBatch batch;
  batch.source_layer = source;
  out->batches.push_back(std::move(batch));
  return &out->batches.back();
}

double polyline_length(const BatchFeature& f) {
  if (f.points.size() < 2) {
    return 0.0;
  }
  double len = 0.0;
  for (size_t i = 1; i < f.points.size(); ++i) {
    const double dx = f.points[i].x - f.points[i - 1].x;
    const double dy = f.points[i].y - f.points[i - 1].y;
    len += std::sqrt(dx * dx + dy * dy);
  }
  return len;
}

int overview_vertex_step(int n, double scale) {
  int step = 1;
  if (n > 20 && scale < 24.0) {
    step = (std::max)(1, n / 48);
  }
  if (n > 32 && scale < 16.0) {
    step = (std::max)(step, n / 56);
  }
  if (n > 48 && scale < 12.0) {
    step = (std::max)(step, n / 36);
  }
  if (n > 96 && scale < 10.0) {
    step = (std::max)(step, n / 28);
  }
  if (n > 192 && scale < 7.0) {
    step = (std::max)(step, n / 20);
  }
  return step;
}

constexpr bool kExtentLonlat = true;

using LineStemLens = std::unordered_map<const BatchFeature*, double>;

struct FeatureProbe {
  const BatchFeature* feature = nullptr;
  FeatureFields fields;
  std::string source;
  detail::LineRole role = detail::LineRole::kOther;
  double len_deg = 0.0;
  bool major = false;
  int label_importance = 0;
  bool stem_water = false;
  bool stem_road = false;
};

bool should_keep_probe(const FeatureProbe& p, double scale, bool use_carto_slots,
                       const LineStemLens* line_stems) {
  if (scale <= 0.0) {
    return true;
  }
  const BatchFeature& f = *p.feature;
  if (!use_carto_slots && (f.kind == BatchGeomKind::kPoint ||
                           f.kind == BatchGeomKind::kText)) {
    return true;
  }
  if (p.source == "label" || f.kind == BatchGeomKind::kText ||
      f.kind == BatchGeomKind::kPoint) {
    int min_imp = detail::label_min_importance(scale);
    if (scale < 12.0 && min_imp < 2) {
      min_imp = 2;
    }
    return p.label_importance >= min_imp;
  }
  if (f.kind == BatchGeomKind::kLine) {
    double len_deg = p.len_deg;
    if ((p.role == detail::LineRole::kWater ||
         p.role == detail::LineRole::kRoad) &&
        line_stems) {
      const auto it = line_stems->find(p.feature);
      if (it != line_stems->end() && it->second > len_deg) {
        len_deg = it->second;
      }
    }
    bool major = p.major;
    if (p.role == detail::LineRole::kRoad && scale < 16.0) {
      auto is_secondary = [](const char* s) {
        if (!s || !s[0]) {
          return false;
        }
        return std::strstr(s, "secondary") != nullptr ||
               std::strstr(s, "Secondary") != nullptr;
      };
      if (is_secondary(p.fields.kind) ||
          is_secondary(feature_class_cstr(p.fields))) {
        major = false;
      }
    }
    return detail::line_visible_at_scale(p.role, len_deg, major, scale);
  }
  return true;
}

void fill_stem_lens_from_entries(
    std::vector<std::pair<const BatchFeature*, std::string>>* named,
    std::vector<detail::StemSpan>* spans, LineStemLens* out) {
  if (!named || !spans || !out || named->empty()) {
    return;
  }
  for (size_t i = 0; i < named->size(); ++i) {
    (*spans)[i].name = (*named)[i].second.c_str();
  }
  std::vector<double> stem_lens(spans->size(), 0.0);
  detail::fill_stem_lengths(spans->data(), spans->size(), 0.05, stem_lens.data());
  for (size_t i = 0; i < named->size(); ++i) {
    out->emplace((*named)[i].first, stem_lens[i]);
  }
}

LineStemLens build_line_stem_lengths(const std::vector<FeatureProbe>& probes) {
  struct StemBucket {
    std::vector<std::pair<const BatchFeature*, std::string>> named;
    std::vector<detail::StemSpan> spans;
  };
  StemBucket water;
  StemBucket road;
  auto push_entry = [](StemBucket* bucket, const FeatureProbe& p) {
    const BatchFeature& f = *p.feature;
    detail::StemSpan span;
    span.length = p.len_deg;
    span.x0 = f.points.front().x;
    span.y0 = f.points.front().y;
    span.x1 = f.points.back().x;
    span.y1 = f.points.back().y;
    bucket->named.emplace_back(p.feature, feature_display_name(p.fields));
    bucket->spans.push_back(span);
  };
  for (const FeatureProbe& p : probes) {
    if (p.stem_water) {
      push_entry(&water, p);
    } else if (p.stem_road) {
      push_entry(&road, p);
    }
  }
  LineStemLens out;
  fill_stem_lens_from_entries(&water.named, &water.spans, &out);
  fill_stem_lens_from_entries(&road.named, &road.spans, &out);
  return out;
}

FeatureProbe make_probe(const BatchFeature& f, const std::string& layer_slot,
                        bool layer_is_slot, bool use_carto_slots) {
  FeatureProbe p;
  p.feature = &f;
  p.fields = probe_fields(f);
  p.source = !use_carto_slots
                 ? layer_slot
                 : (layer_is_slot ? layer_slot : carto_source_layer(f, p.fields));
  const char* cls = feature_class_cstr(p.fields);
  p.role = detail::line_role(p.fields.kind, cls);
  p.major = detail::line_is_major_class(p.fields.kind, cls);
  if (f.kind == BatchGeomKind::kLine && f.points.size() >= 2) {
    p.len_deg = detail::length_as_degrees(polyline_length(f), kExtentLonlat);
    p.stem_water =
        (p.source == "river") || (p.role == detail::LineRole::kWater);
    p.stem_road = (p.source == "road") || (p.role == detail::LineRole::kRoad);
  }
  if (p.source == "label" || f.kind == BatchGeomKind::kText ||
      f.kind == BatchGeomKind::kPoint) {
    p.label_importance = feature_label_importance(p.fields);
  }
  return p;
}

void append_probe_geometry(const FeatureProbe& p, double scale,
                           LayerBatchSet* out) {
  if (!out || !p.feature) {
    return;
  }
  const BatchFeature& f = *p.feature;
  LayerBatch* batch = batch_for(out, p.source);
  std::unique_ptr<OGRGeometry> geom;
  const int n = static_cast<int>(f.points.size());
  const int step = (f.kind == BatchGeomKind::kLine ||
                    f.kind == BatchGeomKind::kPolygon)
                       ? overview_vertex_step(n, scale)
                       : 1;
  const int safe_step =
      (f.kind == BatchGeomKind::kPolygon && n / (std::max)(step, 1) < 3)
          ? 1
          : ((f.kind == BatchGeomKind::kLine && n / (std::max)(step, 1) < 2)
                 ? 1
                 : step);
  if (f.kind == BatchGeomKind::kLine) {
    auto line = std::make_unique<OGRLineString>();
    for (int i = 0; i < n; i += safe_step) {
      line->addPoint(f.points[static_cast<size_t>(i)].x,
                     f.points[static_cast<size_t>(i)].y);
    }
    if (n > 0 && (n - 1) % safe_step != 0) {
      line->addPoint(f.points.back().x, f.points.back().y);
    }
    geom = std::move(line);
  } else if (f.kind == BatchGeomKind::kPolygon) {
    auto ring = std::make_unique<OGRLinearRing>();
    for (int i = 0; i < n; i += safe_step) {
      ring->addPoint(f.points[static_cast<size_t>(i)].x,
                     f.points[static_cast<size_t>(i)].y);
    }
    if (n > 0 && (n - 1) % safe_step != 0) {
      ring->addPoint(f.points.back().x, f.points.back().y);
    }
    ring->closeRings();
    auto poly = std::make_unique<OGRPolygon>();
    poly->addRingDirectly(ring.release());
    geom = std::move(poly);
  } else {
    geom = std::make_unique<OGRPoint>(f.points.front().x, f.points.front().y);
  }
  batch->geoms.push_back(geom.get());
  out->owned.push_back(std::move(geom));
  std::map<std::string, std::string> attrs;
  for (const BatchField& field : f.fields) {
    attrs.emplace(field.name, field.value);
  }
  batch->attrs.push_back(std::move(attrs));
}

void collect_layer_probes(const BatchLayer& layer, bool use_carto_slots,
                          std::vector<FeatureProbe>* probes) {
  if (!probes || !layer.visible) {
    return;
  }
  const std::string layer_slot = layer.name.empty() ? layer.id : layer.name;
  const bool layer_is_slot = use_carto_slots && is_carto_slot(layer_slot);
  probes->reserve(probes->size() + layer.features.size());
  for (const BatchFeature& f : layer.features) {
    if (f.points.empty()) {
      continue;
    }
    probes->push_back(make_probe(f, layer_slot, layer_is_slot, use_carto_slots));
  }
}

void emit_kept_probes(const std::vector<FeatureProbe>& probes, double scale,
                      bool use_carto_slots, const LineStemLens* line_stems,
                      LayerBatchSet* out) {
  if (!out) {
    return;
  }
  for (const FeatureProbe& probe : probes) {
    if (!should_keep_probe(probe, scale, use_carto_slots, line_stems)) {
      continue;
    }
    append_probe_geometry(probe, scale, out);
  }
}

}  // namespace

LayerBatchSet::LayerBatchSet() = default;

LayerBatchSet::~LayerBatchSet() = default;

LayerBatchSet::LayerBatchSet(LayerBatchSet&& other) noexcept
    : batches(std::move(other.batches)), owned(std::move(other.owned)) {}

LayerBatchSet& LayerBatchSet::operator=(LayerBatchSet&& other) noexcept {
  batches = std::move(other.batches);
  owned = std::move(other.owned);
  return *this;
}

LayerBatchSet build_layer_batches(const std::vector<BatchLayer>& layers,
                                  bool use_carto_slots, double scale) {
  std::vector<size_t> visible;
  visible.reserve(layers.size());
  for (size_t i = 0; i < layers.size(); ++i) {
    if (layers[i].visible) {
      visible.push_back(i);
    }
  }
  if (visible.empty()) {
    return {};
  }
  std::vector<FeatureProbe> probes;
  size_t feature_hint = 0;
  for (size_t vi : visible) {
    feature_hint += layers[vi].features.size();
  }
  probes.reserve(feature_hint);
  for (size_t vi : visible) {
    collect_layer_probes(layers[vi], use_carto_slots, &probes);
  }
  const LineStemLens line_stems = build_line_stem_lengths(probes);
  LayerBatchSet out;
  out.batches.reserve(8);
  emit_kept_probes(probes, scale, use_carto_slots, &line_stems, &out);
  return out;
}

}  // namespace vista
