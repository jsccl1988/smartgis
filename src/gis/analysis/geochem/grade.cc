// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/grade.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace gis {
namespace detail {
namespace {

uint32_t lerp_rgba(uint32_t a, uint32_t b, double t) {
  t = std::clamp(t, 0.0, 1.0);
  auto ch = [&](int shift) {
    const int ca = static_cast<int>((a >> shift) & 0xff);
    const int cb = static_cast<int>((b >> shift) & 0xff);
    return static_cast<uint32_t>(
        std::lround(ca + (cb - ca) * t));
  };
  return (0xffu << 24) | (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

// Blue → cyan → yellow → red ramp (low → high).
uint32_t ramp_color(double t) {
  t = std::clamp(t, 0.0, 1.0);
  constexpr uint32_t kBlue = 0xff2166ac;
  constexpr uint32_t kCyan = 0xff67a9cf;
  constexpr uint32_t kYellow = 0xfffee08b;
  constexpr uint32_t kRed = 0xffd73027;
  if (t < 0.33) {
    return lerp_rgba(kBlue, kCyan, t / 0.33);
  }
  if (t < 0.66) {
    return lerp_rgba(kCyan, kYellow, (t - 0.33) / 0.33);
  }
  return lerp_rgba(kYellow, kRed, (t - 0.66) / 0.34);
}

}  // namespace

std::string geochem_rgba_to_hex(uint32_t rgba) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "#%02X%02X%02X",
                static_cast<int>((rgba >> 16) & 0xff),
                static_cast<int>((rgba >> 8) & 0xff),
                static_cast<int>(rgba & 0xff));
  return std::string(buf);
}

int geochem_grade_class_index(const GeochemGradeLegend& legend, double value) {
  if (!legend.ok || !std::isfinite(value)) {
    return -1;
  }
  for (size_t i = 0; i < legend.classes.size(); ++i) {
    const GeochemGradeClass& c = legend.classes[i];
    if (value >= c.lo && (value < c.hi || i + 1 == legend.classes.size())) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

GeochemGradeLegend build_geochem_grade_legend(const GeochemSampleSet& set,
                                              std::string_view element,
                                              int class_count) {
  GeochemGradeLegend out;
  out.element = std::string(element);
  std::vector<double> vals;
  if (!geochem_values_for_element(set, element, &vals)) {
    out.error = "element_missing";
    return out;
  }
  if (class_count < 2) {
    class_count = 5;
  }
  if (class_count > 12) {
    class_count = 12;
  }
  const double lo = *std::min_element(vals.begin(), vals.end());
  const double hi = *std::max_element(vals.begin(), vals.end());
  const double span = std::max(1e-12, hi - lo);
  out.classes.resize(static_cast<size_t>(class_count));
  for (int i = 0; i < class_count; ++i) {
    GeochemGradeClass& c = out.classes[static_cast<size_t>(i)];
    c.lo = lo + span * static_cast<double>(i) / class_count;
    c.hi = lo + span * static_cast<double>(i + 1) / class_count;
    if (i + 1 == class_count) {
      c.hi = hi;
    }
    const double t =
        class_count == 1
            ? 0.5
            : static_cast<double>(i) / static_cast<double>(class_count - 1);
    c.rgba = ramp_color(t);
    char label[64];
    std::snprintf(label, sizeof(label), "%.3g – %.3g", c.lo, c.hi);
    c.label = label;
  }
  out.ok = true;
  return out;
}

double geochem_heat_score(double value, double min_v, double max_v) {
  if (!std::isfinite(value)) {
    return 0.0;
  }
  if (!std::isfinite(min_v) || !std::isfinite(max_v) || max_v <= min_v) {
    return 50.0;
  }
  const double t = (value - min_v) / (max_v - min_v);
  return std::clamp(t, 0.0, 1.0) * 100.0;
}

const char* format_geochem_heat(double score, char* buf, size_t cap) {
  if (!buf || cap < 4) {
    return "";
  }
  double v = score;
  if (!std::isfinite(v) || v < 0.0) {
    v = 0.0;
  }
  if (v > 100.0) {
    v = 100.0;
  }
  const int n = std::snprintf(buf, cap, "%.4f", v);
  if (n <= 0 || static_cast<size_t>(n) >= cap) {
    buf[0] = '\0';
    return "";
  }
  return buf;
}

}  // namespace detail
}  // namespace gis
