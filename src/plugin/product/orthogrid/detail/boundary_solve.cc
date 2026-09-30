// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid/detail/boundary_solve.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "plugin/product/orthogrid/detail/laplace_solver.h"

namespace plugin {
namespace detail {
namespace {

constexpr int kMaxNodes = 512 * 512;

struct Pt {
  double x = 0.0;
  double y = 0.0;
};

struct Edge {
  int start = 0;
  int end = 0;
  int index = 0;
  int flag = -1;
  std::vector<Pt> pts;
};

std::string err(const char* code) {
  return std::string("{\"error\":\"") + code + "\"}";
}

std::string strip_cr(std::string line) {
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  return line;
}

bool read_line(std::ifstream& in, std::string* line) {
  if (!std::getline(in, *line)) {
    return false;
  }
  *line = strip_cr(std::move(*line));
  return true;
}

Pt sample_polyline(const std::vector<Pt>& pts, double t) {
  if (pts.empty()) {
    return {};
  }
  if (pts.size() == 1 || t <= 0.0) {
    return pts.front();
  }
  if (t >= 1.0) {
    return pts.back();
  }
  const double scaled = t * static_cast<double>(pts.size() - 1);
  const int i = static_cast<int>(scaled);
  const double f = scaled - static_cast<double>(i);
  const int j = i + 1 < static_cast<int>(pts.size()) ? i + 1 : i;
  Pt p;
  p.x = pts[static_cast<size_t>(i)].x +
        (pts[static_cast<size_t>(j)].x - pts[static_cast<size_t>(i)].x) * f;
  p.y = pts[static_cast<size_t>(i)].y +
        (pts[static_cast<size_t>(j)].y - pts[static_cast<size_t>(i)].y) * f;
  return p;
}

bool parse_point(std::string_view line, Pt* out) {
  double x = 0.0;
  double y = 0.0;
  const std::string tmp(line);
  if (std::sscanf(tmp.c_str(), "%lf,%lf", &x, &y) != 2) {
    return false;
  }
  out->x = x;
  out->y = y;
  return true;
}

bool parse_edges(std::ifstream& in, std::vector<Edge>* edges) {
  std::string line;
  if (!read_line(in, &line)) {
    return false;
  }
  while (line != "main_begin") {
    if (!read_line(in, &line)) {
      return false;
    }
  }
  if (!read_line(in, &line)) {
    return false;
  }
  while (line != "main_end") {
    int n = 0;
    if (std::sscanf(line.c_str(), "%d", &n) != 1 || n < 2) {
      return false;
    }
    if (!read_line(in, &line)) {
      return false;
    }
    Edge edge;
    int slide = 0;
    int sample = 0;
    if (std::sscanf(line.c_str(), "%d %d %d %d %d %d", &edge.start, &edge.end,
                    &edge.index, &edge.flag, &slide, &sample) != 6) {
      return false;
    }
    if (edge.flag < 0 || edge.flag > 3) {
      return false;
    }
    edge.pts.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      if (!read_line(in, &line)) {
        return false;
      }
      Pt pt;
      if (!parse_point(line, &pt)) {
        return false;
      }
      edge.pts.push_back(pt);
    }
    edges->push_back(std::move(edge));
    if (!read_line(in, &line)) {
      return false;
    }
  }
  return true;
}

bool paint_edge(const Edge& edge,
                int nx,
                int ny,
                std::vector<double>* xs,
                std::vector<double>* ys,
                std::vector<std::uint8_t>* unknown) {
  const int span = edge.end - edge.start;
  const int count = span >= 0 ? span + 1 : -span + 1;
  if (count <= 0) {
    return false;
  }
  const int step = edge.end >= edge.start ? 1 : -1;
  for (int k = 0; k < count; ++k) {
    const double t =
        count == 1 ? 0.0 : static_cast<double>(k) / static_cast<double>(count - 1);
    const Pt p = sample_polyline(edge.pts, t);
    int i = 0;
    int j = 0;
    if (edge.flag == 0 || edge.flag == 2) {
      i = edge.start + step * k;
      j = edge.index;
    } else {
      j = edge.start + step * k;
      i = edge.index;
    }
    if (i < 0 || j < 0 || i >= nx || j >= ny) {
      return false;
    }
    const size_t at = static_cast<size_t>(j * nx + i);
    (*xs)[at] = p.x;
    (*ys)[at] = p.y;
    (*unknown)[at] = 0;
  }
  return true;
}

bool border_is_dirichlet(int nx,
                         int ny,
                         const std::vector<std::uint8_t>& unknown) {
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const bool border = i == 0 || j == 0 || i == nx - 1 || j == ny - 1;
      if (!border) {
        continue;
      }
      if (unknown[static_cast<size_t>(j * nx + i)] != 0) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

BoundarySolve solve_grid_boundary_file(const std::string& path) {
  BoundarySolve out;
  if (path.empty()) {
    out.message = err("bad_header");
    return out;
  }
  std::ifstream in(path);
  if (!in) {
    out.message = err("bad_header");
    return out;
  }
  std::string line;
  if (!read_line(in, &line) || line != "gridbnd:") {
    out.message = err("bad_header");
    return out;
  }
  if (!read_line(in, &line)) {
    out.message = err("bad_size");
    return out;
  }
  int nx = 0;
  int ny = 0;
  if (std::sscanf(line.c_str(), "%d %d", &nx, &ny) != 2 || nx < 3 || ny < 3) {
    out.message = err("bad_size");
    return out;
  }
  const int64_t nodes = static_cast<int64_t>(nx) * static_cast<int64_t>(ny);
  if (nodes <= 0 || nodes > kMaxNodes) {
    out.message = err("grid_too_large");
    return out;
  }

  std::vector<Edge> edges;
  if (!parse_edges(in, &edges)) {
    out.message = err("no_main_region");
    return out;
  }
  bool seen[4] = {};
  for (const Edge& edge : edges) {
    if (edge.flag < 0 || edge.flag > 3 || seen[edge.flag]) {
      out.message = err("boundary_unsolved");
      return out;
    }
    seen[edge.flag] = true;
  }
  if (!seen[0] || !seen[1] || !seen[2] || !seen[3]) {
    out.message = err("boundary_unsolved");
    return out;
  }

  const size_t n = static_cast<size_t>(nodes);
  std::vector<double> xs(n, 0.0);
  std::vector<double> ys(n, 0.0);
  std::vector<std::uint8_t> unknown(n, 1);
  for (const Edge& edge : edges) {
    if (!paint_edge(edge, nx, ny, &xs, &ys, &unknown)) {
      out.message = err("boundary_unsolved");
      return out;
    }
  }
  if (!border_is_dirichlet(nx, ny, unknown)) {
    out.message = err("boundary_unsolved");
    return out;
  }

  orthogrid::GridField field;
  field.nx = nx;
  field.ny = ny;
  field.x = xs.data();
  field.y = ys.data();
  if (!orthogrid::solve_laplace(field, unknown.data())) {
    out.message = err("laplace_failed");
    return out;
  }

  out.ok = true;
  out.nx = nx;
  out.ny = ny;
  out.node_count = static_cast<int>(nodes);
  out.xs = std::move(xs);
  out.ys = std::move(ys);
  out.message = std::string("{\"ok\":true,\"nodes\":") +
                std::to_string(out.node_count) + "}";
  return out;
}

}  // namespace detail
}  // namespace plugin
