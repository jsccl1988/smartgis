// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CRS checks and short result strings shared by the IR boundary.
// These helpers do not parse or emit WKT.

#ifndef GIS_ANALYSIS_OPS_IR_TEXT_H_
#define GIS_ANALYSIS_OPS_IR_TEXT_H_

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace gis {
namespace detail {

inline bool reject(std::string* error, const char* reason) {
  if (error) {
    *error = reason && reason[0] ? reason : "failed";
  }
  return false;
}

inline bool accept(std::string* out, std::string value) {
  if (!out) {
    return false;
  }
  *out = std::move(value);
  return true;
}

inline std::string_view trim_ascii(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
    text.remove_suffix(1);
  }
  return text;
}

inline bool parse_finite(std::string_view text, double* out) {
  text = trim_ascii(text);
  if (text.empty() || !out) {
    return false;
  }
  double value = 0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite(value)) {
    return false;
  }
  *out = value;
  return true;
}

inline bool is_geographic_epsg(int32_t epsg) {
  return epsg >= 4000 && epsg <= 4999;
}

inline bool require_epsg(int32_t epsg, int32_t* crs, std::string* error) {
  if (epsg == 0) {
    return reject(error, "unknown crs");
  }
  if (crs) {
    *crs = epsg;
  }
  return true;
}

inline bool require_same_epsg(int32_t left, int32_t right, int32_t* crs, std::string* error) {
  if (!require_epsg(left, nullptr, error) || !require_epsg(right, nullptr, error)) {
    return false;
  }
  if (left != right) {
    return reject(error, "crs mismatch");
  }
  if (crs) {
    *crs = left;
  }
  return true;
}

inline std::string format_number(double value) {
  char buf[64];
  const std::to_chars_result written =
      std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::general, 16);
  if (written.ec != std::errc{}) {
    return "0";
  }
  return std::string(buf, written.ptr);
}

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_IR_TEXT_H_
