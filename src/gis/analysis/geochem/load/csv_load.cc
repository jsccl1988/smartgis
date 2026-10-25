// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/load/csv_load.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace gis {
namespace detail {
namespace {

std::string trim(std::string_view s) {
  size_t b = 0;
  while (b < s.size() &&
         std::isspace(static_cast<unsigned char>(s[b]))) {
    ++b;
  }
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
    --e;
  }
  return std::string(s.substr(b, e - b));
}

std::string lower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

std::vector<std::string> split_csv_line(const std::string& line) {
  std::vector<std::string> fields;
  std::string cur;
  bool in_quotes = false;
  for (size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '"') {
      in_quotes = !in_quotes;
      continue;
    }
    if ((c == ',' || c == ';' || c == '\t') && !in_quotes) {
      fields.push_back(trim(cur));
      cur.clear();
      continue;
    }
    cur.push_back(c);
  }
  fields.push_back(trim(cur));
  return fields;
}

bool parse_double(const std::string& s, double* out) {
  if (!out || s.empty()) {
    return false;
  }
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str()) {
    return false;
  }
  *out = v;
  return std::isfinite(v);
}

int find_alias(const std::vector<std::string>& header,
               const std::vector<std::string>& aliases) {
  for (size_t i = 0; i < header.size(); ++i) {
    const std::string h = lower(header[i]);
    for (const std::string& a : aliases) {
      if (h == a) {
        return static_cast<int>(i);
      }
    }
  }
  return -1;
}

bool is_reserved_col(const std::string& name) {
  const std::string h = lower(name);
  return h == "lon" || h == "longitude" || h == "lng" || h == "x" ||
         h == "lat" || h == "latitude" || h == "y" || h == "id" ||
         h == "sample_id" || h == "sid" || h == "name";
}

}  // namespace

GeochemSampleSet load_csv(std::string_view path) {
  GeochemSampleSet out;
  if (path.empty()) {
    out.error = "empty path";
    return out;
  }
  const std::string path_str(path);
  std::ifstream in;
  in.open(path_str.c_str());
  if (!in.is_open()) {
    out.error = "failed to open CSV";
    return out;
  }
  std::string line;
  if (!std::getline(in, line)) {
    out.error = "empty CSV";
    return out;
  }
  if (line.size() >= 3 &&
      static_cast<unsigned char>(line[0]) == 0xEF &&
      static_cast<unsigned char>(line[1]) == 0xBB &&
      static_cast<unsigned char>(line[2]) == 0xBF) {
    line.erase(0, 3);
  }
  const std::vector<std::string> header = split_csv_line(line);
  const int col_x = find_alias(header, {"lon", "longitude", "lng", "x"});
  const int col_y = find_alias(header, {"lat", "latitude", "y"});
  if (col_x < 0 || col_y < 0) {
    out.error = "missing lon/lat columns";
    return out;
  }
  const int col_id = find_alias(header, {"id", "sample_id", "sid", "name"});

  std::vector<int> elem_cols;
  for (size_t i = 0; i < header.size(); ++i) {
    if (static_cast<int>(i) == col_x || static_cast<int>(i) == col_y ||
        static_cast<int>(i) == col_id) {
      continue;
    }
    if (header[i].empty() || is_reserved_col(header[i])) {
      continue;
    }
    elem_cols.push_back(static_cast<int>(i));
    out.element_names.push_back(header[i]);
  }
  if (elem_cols.empty()) {
    out.error = "no element columns";
    return out;
  }

  int row_i = 0;
  while (std::getline(in, line)) {
    if (trim(line).empty()) {
      continue;
    }
    const std::vector<std::string> fields = split_csv_line(line);
    double x = 0;
    double y = 0;
    if (col_x >= static_cast<int>(fields.size()) ||
        col_y >= static_cast<int>(fields.size()) ||
        !parse_double(fields[static_cast<size_t>(col_x)], &x) ||
        !parse_double(fields[static_cast<size_t>(col_y)], &y)) {
      continue;
    }
    GeochemSample sample;
    sample.x = x;
    sample.y = y;
    if (col_id >= 0 && col_id < static_cast<int>(fields.size())) {
      sample.id = fields[static_cast<size_t>(col_id)];
    } else {
      sample.id = "S" + std::to_string(++row_i);
    }
    sample.values.resize(elem_cols.size(),
                         std::numeric_limits<double>::quiet_NaN());
    for (size_t e = 0; e < elem_cols.size(); ++e) {
      const int c = elem_cols[e];
      double v = 0;
      if (c < static_cast<int>(fields.size()) &&
          parse_double(fields[static_cast<size_t>(c)], &v)) {
        sample.values[e] = v;
      }
    }
    out.samples.push_back(std::move(sample));
  }
  if (out.samples.empty()) {
    out.error = "no samples";
    return out;
  }
  out.ok = true;
  return out;
}

}  // namespace detail
}  // namespace gis
