// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/tin/xyz_points.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace tin {
namespace {

bool is_blank_line(const std::string& line) {
  for (unsigned char ch : line) {
    if (!std::isspace(ch)) {
      return false;
    }
  }
  return true;
}

void split_fields(const std::string& line,
                  char separator,
                  std::vector<std::string>* fields) {
  fields->clear();
  std::string cur;
  for (char ch : line) {
    if (ch == separator) {
      if (!cur.empty() || separator != ' ') {
        fields->push_back(cur);
      }
      cur.clear();
      continue;
    }
    cur.push_back(ch);
  }
  if (!cur.empty() || (!fields->empty() && separator != ' ')) {
    fields->push_back(cur);
  }
}

bool parse_coord(const std::string& text, float* out) {
  if (text.empty() || out == nullptr) {
    return false;
  }
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end == text.c_str()) {
    return false;
  }
  while (end && *end != '\0') {
    if (!std::isspace(static_cast<unsigned char>(*end))) {
      return false;
    }
    ++end;
  }
  *out = static_cast<float>(value);
  return true;
}

}  // namespace

long read_xyz_points(const char* path,
                     int skip_header_lines,
                     char separator,
                     int x_col,
                     int y_col,
                     int z_col,
                     std::vector<render::Vector3>* out) {
  if (path == nullptr || path[0] == '\0' || out == nullptr ||
      skip_header_lines < 0 || x_col < 0 || y_col < 0 || z_col < 0) {
    return SMT_ERR_INVALID_PARAM;
  }

  std::ifstream in(path);
  if (!in) {
    return SMT_ERR_INVALID_FILE;
  }

  out->clear();
  std::string line;
  int skipped = 0;
  std::vector<std::string> fields;
  const int need = x_col;
  const int need_y = y_col;
  const int need_z = z_col;
  const int min_cols = (need > need_y ? need : need_y);
  const int required = (min_cols > need_z ? min_cols : need_z) + 1;

  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (skipped < skip_header_lines) {
      ++skipped;
      continue;
    }
    if (is_blank_line(line)) {
      continue;
    }
    split_fields(line, separator, &fields);
    if (static_cast<int>(fields.size()) < required) {
      return SMT_ERR_INVALID_PARAM;
    }
    render::Vector3 pt;
    if (!parse_coord(fields[static_cast<std::size_t>(x_col)], &pt.x) ||
        !parse_coord(fields[static_cast<std::size_t>(y_col)], &pt.y) ||
        !parse_coord(fields[static_cast<std::size_t>(z_col)], &pt.z)) {
      return SMT_ERR_INVALID_PARAM;
    }
    out->push_back(pt);
  }
  return SMT_ERR_NONE;
}

}  // namespace tin
