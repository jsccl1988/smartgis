// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/orthogrid/solve/boundary_solve.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include "gis/geo/grid/laplace.h"
#include "gis/geo/grid/orthogonality.h"

namespace plugin {
namespace detail {
namespace {

constexpr int kMaxNodes = 512 * 512;
constexpr int kDefaultRaster = 96;

using Pt = Eigen::Vector2d;

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
    return Pt::Zero();
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
  return pts[static_cast<size_t>(i)] +
         f * (pts[static_cast<size_t>(j)] - pts[static_cast<size_t>(i)]);
}

bool parse_point(std::string_view line, Pt* out) {
  double x = 0.0;
  double y = 0.0;
  const std::string tmp(line);
  if (std::sscanf(tmp.c_str(), "%lf,%lf", &x, &y) != 2) {
    return false;
  }
  *out = Pt(x, y);
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
    (*xs)[at] = p.x();
    (*ys)[at] = p.y();
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

void fill_orth_fields(geo::NodeField2d field, BoundarySolve* out) {
  if (!out || !out->ok) {
    return;
  }
  const geo::Orthogonality2d orth = geo::compute_orthogonality(field);
  out->node_orth = orth.node_delta;
  out->cell_orth = orth.cell_delta;
  const int rw = std::min(kDefaultRaster, std::max(out->nx * 2, 16));
  const int rh = std::min(kDefaultRaster, std::max(out->ny * 2, 16));
  if (geo::sample_orthogonality_raster(
          field, out->node_orth.data(), rw, rh, &out->raster_orth,
          &out->raster_min_x, &out->raster_min_y, &out->raster_max_x,
          &out->raster_max_y)) {
    out->raster_w = rw;
    out->raster_h = rh;
  }
}

BoundarySolve solve_from_painted(int nx,
                                 int ny,
                                 std::vector<double> xs,
                                 std::vector<double> ys,
                                 std::vector<std::uint8_t> unknown,
                                 int elliptic_iters) {
  BoundarySolve out;
  out.elliptic_iters = std::max(0, elliptic_iters);
  geo::NodeField2d field{nx, ny, xs.data(), ys.data()};
  if (!geo::solve_laplace(field, unknown.data())) {
    out.message = err("laplace_failed");
    return out;
  }
  auto push_frame = [&]() {
    out.frame_xs.push_back(xs);
    out.frame_ys.push_back(ys);
  };
  push_frame();
  for (int s = 0; s < out.elliptic_iters; ++s) {
    if (!geo::solve_elliptic(field, unknown.data(), 1)) {
      out.message = err("elliptic_failed");
      return out;
    }
    push_frame();
  }
  out.ok = true;
  out.nx = nx;
  out.ny = ny;
  out.node_count = nx * ny;
  out.xs = xs;
  out.ys = ys;
  fill_orth_fields(field, &out);
  out.message = std::string("{\"ok\":true,\"nodes\":") +
                std::to_string(out.node_count) + ",\"elliptic_iters\":" +
                std::to_string(out.elliptic_iters) + ",\"frames\":" +
                std::to_string(out.frame_xs.size()) + "}";
  return out;
}

Edge edge_from_boundary(const BoundaryEdge& be, int nx, int ny) {
  Edge edge;
  edge.flag = be.flag;
  edge.pts.reserve(be.pts.size());
  for (const auto& p : be.pts) {
    edge.pts.emplace_back(p.first, p.second);
  }
  const int n = static_cast<int>(be.pts.size());
  if (be.flag == 0) {
    edge.start = 0;
    edge.end = nx - 1;
    edge.index = 0;
  } else if (be.flag == 1) {
    edge.start = 0;
    edge.end = ny - 1;
    edge.index = nx - 1;
  } else if (be.flag == 2) {
    edge.start = nx - 1;
    edge.end = 0;
    edge.index = ny - 1;
  } else {
    edge.start = ny - 1;
    edge.end = 0;
    edge.index = 0;
  }
  (void)n;
  return edge;
}

}  // namespace

BoundarySolve solve_grid_boundary(const std::vector<BoundaryEdge>& edges,
                                  int nx,
                                  int ny,
                                  int elliptic_iters) {
  BoundarySolve out;
  bool seen[4] = {};
  for (const BoundaryEdge& e : edges) {
    if (e.flag < 0 || e.flag > 3 || e.pts.size() < 2 || seen[e.flag]) {
      out.message = err("boundary_unsolved");
      return out;
    }
    seen[e.flag] = true;
  }
  if (!seen[0] || !seen[1] || !seen[2] || !seen[3]) {
    out.message = err("boundary_unsolved");
    return out;
  }

  int use_nx = nx;
  int use_ny = ny;
  if (use_nx < 3 || use_ny < 3) {
    use_nx = 3;
    use_ny = 3;
    for (const BoundaryEdge& e : edges) {
      const int n = static_cast<int>(e.pts.size());
      if (e.flag == 0 || e.flag == 2) {
        use_nx = std::max(use_nx, std::max(n, 3));
      } else {
        use_ny = std::max(use_ny, std::max(n, 3));
      }
    }
  }
  const int64_t nodes =
      static_cast<int64_t>(use_nx) * static_cast<int64_t>(use_ny);
  if (nodes <= 0 || nodes > kMaxNodes) {
    out.message = err("grid_too_large");
    return out;
  }

  const size_t n = static_cast<size_t>(nodes);
  std::vector<double> xs(n, 0.0);
  std::vector<double> ys(n, 0.0);
  std::vector<std::uint8_t> unknown(n, 1);
  for (const BoundaryEdge& be : edges) {
    const Edge edge = edge_from_boundary(be, use_nx, use_ny);
    if (!paint_edge(edge, use_nx, use_ny, &xs, &ys, &unknown)) {
      out.message = err("boundary_unsolved");
      return out;
    }
  }
  if (!border_is_dirichlet(use_nx, use_ny, unknown)) {
    out.message = err("boundary_unsolved");
    return out;
  }
  return solve_from_painted(use_nx, use_ny, std::move(xs), std::move(ys),
                            std::move(unknown), elliptic_iters);
}

BoundarySolve solve_grid_boundary_file(const std::string& path,
                                       int elliptic_iters) {
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
  return solve_from_painted(nx, ny, std::move(xs), std::move(ys),
                            std::move(unknown), elliptic_iters);
}

}  // namespace detail
}  // namespace plugin
