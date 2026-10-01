// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geology/borehole.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
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
    if (c == ',' && !in_quotes) {
      fields.push_back(trim(cur));
      cur.clear();
      continue;
    }
    cur.push_back(c);
  }
  fields.push_back(trim(cur));
  return fields;
}

int find_column(const std::vector<std::string>& header, const char* name) {
  for (size_t i = 0; i < header.size(); ++i) {
    if (header[i] == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
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
  return true;
}

}  // namespace

BoreholeSet load_boreholes_csv(std::string_view path) {
  BoreholeSet out;
  if (path.empty()) {
    out.error = "empty path";
    return out;
  }
  const std::string path_str(path);
  std::ifstream in(path_str.c_str());
  if (!in) {
    out.error = "failed to open CSV";
    return out;
  }

  std::string line;
  if (!std::getline(in, line)) {
    out.error = "empty CSV";
    return out;
  }
  // Strip UTF-8 BOM if present.
  if (line.size() >= 3 &&
      static_cast<unsigned char>(line[0]) == 0xEF &&
      static_cast<unsigned char>(line[1]) == 0xBB &&
      static_cast<unsigned char>(line[2]) == 0xBF) {
    line.erase(0, 3);
  }
  const std::vector<std::string> header = split_csv_line(line);
  const int col_hole = find_column(header, "hole_id");
  const int col_x = find_column(header, "x");
  const int col_y = find_column(header, "y");
  const int col_z = find_column(header, "z");
  const int col_stratum = find_column(header, "stratum_id");
  if (col_hole < 0 || col_x < 0 || col_y < 0 || col_z < 0 || col_stratum < 0) {
    out.error = "CSV header must include hole_id,x,y,z,stratum_id";
    return out;
  }

  int row = 1;
  while (std::getline(in, line)) {
    ++row;
    if (trim(line).empty()) {
      continue;
    }
    const std::vector<std::string> fields = split_csv_line(line);
    const int need = col_stratum;
    if (static_cast<int>(fields.size()) <= need) {
      out.error = "row " + std::to_string(row) + ": too few columns";
      out.contacts.clear();
      return out;
    }
    BoreholeContact c;
    c.hole_id = fields[static_cast<size_t>(col_hole)];
    c.stratum_id = fields[static_cast<size_t>(col_stratum)];
    if (c.hole_id.empty() || c.stratum_id.empty()) {
      out.error = "row " + std::to_string(row) + ": empty hole_id/stratum_id";
      out.contacts.clear();
      return out;
    }
    if (!parse_double(fields[static_cast<size_t>(col_x)], &c.x) ||
        !parse_double(fields[static_cast<size_t>(col_y)], &c.y) ||
        !parse_double(fields[static_cast<size_t>(col_z)], &c.z)) {
      out.error = "row " + std::to_string(row) + ": bad x/y/z";
      out.contacts.clear();
      return out;
    }
    out.contacts.push_back(std::move(c));
  }

  if (out.contacts.empty()) {
    out.error = "no contact rows";
    return out;
  }
  out.ok = true;
  return out;
}

}  // namespace detail
}  // namespace gis
