// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/hexgrid/solve/boundary_solve.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "gis/geo/grid/laplace.h"
#include "gis/geo/grid/orthogonality.h"

namespace plugin {
namespace detail {
namespace {

Xyz trilinear(const Xyz c[8],
                          double u,
                          double v,
                          double w) {
  // Corner layout: 0(000) 1(100) 2(110) 3(010) 4(001) 5(101) 6(111) 7(011).
  const double ou = 1.0 - u;
  const double ov = 1.0 - v;
  const double ow = 1.0 - w;
  Xyz p;
  p.x = ou * ov * ow * c[0].x + u * ov * ow * c[1].x + u * v * ow * c[2].x +
        ou * v * ow * c[3].x + ou * ov * w * c[4].x + u * ov * w * c[5].x +
        u * v * w * c[6].x + ou * v * w * c[7].x;
  p.y = ou * ov * ow * c[0].y + u * ov * ow * c[1].y + u * v * ow * c[2].y +
        ou * v * ow * c[3].y + ou * ov * w * c[4].y + u * ov * w * c[5].y +
        u * v * w * c[6].y + ou * v * w * c[7].y;
  p.z = ou * ov * ow * c[0].z + u * ov * ow * c[1].z + u * v * ow * c[2].z +
        ou * v * ow * c[3].z + ou * ov * w * c[4].z + u * ov * w * c[5].z +
        u * v * w * c[6].z + ou * v * w * c[7].z;
  return p;
}

}  // namespace

HexCornerSolve solve_hex_from_nodes(int nx,
                                    int ny,
                                    int nz,
                                    std::vector<double> xs,
                                    std::vector<double> ys,
                                    std::vector<double> zs) {
  HexCornerSolve out;
  if (nx < 3 || ny < 3 || nz < 3) {
    out.message = "{\"error\":\"bad_dims\"}";
    return out;
  }
  out.grid.hex_resize(nx, ny, nz);
  const int n = out.grid.node_count();
  if (static_cast<int>(xs.size()) != n || static_cast<int>(ys.size()) != n ||
      static_cast<int>(zs.size()) != n) {
    out.message = "{\"error\":\"bad_seed\"}";
    return out;
  }

  std::vector<std::uint8_t> unknown(static_cast<size_t>(n), 0);
  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const int idx = out.grid.hex_index(i, j, k);
        const bool on_face = (i == 0 || i == nx - 1 || j == 0 || j == ny - 1 ||
                              k == 0 || k == nz - 1);
        unknown[static_cast<size_t>(idx)] = on_face ? 0 : 1;
      }
    }
  }

  geo::NodeField3d field{nx, ny, nz, xs.data(), ys.data(), zs.data()};
  if (!geo::solve_laplace(field, unknown.data())) {
    out.message = "{\"error\":\"laplace_failed\"}";
    return out;
  }

  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const int idx = out.grid.hex_index(i, j, k);
        out.grid.hex_set_point(i, j, k, xs[static_cast<size_t>(idx)],
                               ys[static_cast<size_t>(idx)],
                               zs[static_cast<size_t>(idx)]);
      }
    }
  }

  const geo::Orthogonality3d orth = geo::compute_orthogonality(field);
  out.cell_orth = orth.cell_delta;
  out.ok = true;
  out.message = "{\"ok\":true,\"op\":\"orthogrid3d.create_hex_grid\",\"nx\":" +
                std::to_string(nx) + ",\"ny\":" + std::to_string(ny) +
                ",\"nz\":" + std::to_string(nz) + "}";
  return out;
}

HexCornerSolve solve_hex_from_corners(const Xyz corners[8],
                                      int nx,
                                      int ny,
                                      int nz) {
  HexCornerSolve out;
  if (corners == nullptr || nx < 3 || ny < 3 || nz < 3) {
    out.message = "{\"error\":\"bad_dims\"}";
    return out;
  }

  out.grid.hex_resize(nx, ny, nz);
  const int n = out.grid.node_count();
  std::vector<double> xs(static_cast<size_t>(n));
  std::vector<double> ys(static_cast<size_t>(n));
  std::vector<double> zs(static_cast<size_t>(n));

  for (int k = 0; k < nz; ++k) {
    const double w = static_cast<double>(k) / static_cast<double>(nz - 1);
    for (int j = 0; j < ny; ++j) {
      const double v = static_cast<double>(j) / static_cast<double>(ny - 1);
      for (int i = 0; i < nx; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(nx - 1);
        const Xyz p = trilinear(corners, u, v, w);
        const int idx = out.grid.hex_index(i, j, k);
        xs[static_cast<size_t>(idx)] = p.x;
        ys[static_cast<size_t>(idx)] = p.y;
        zs[static_cast<size_t>(idx)] = p.z;
      }
    }
  }

  return solve_hex_from_nodes(nx, ny, nz, std::move(xs), std::move(ys),
                              std::move(zs));
}

}  // namespace detail
}  // namespace plugin
