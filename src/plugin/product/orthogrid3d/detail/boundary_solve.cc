// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid3d/detail/boundary_solve.h"

#include <cstdint>
#include <vector>

#include "plugin/product/orthogrid3d/detail/laplace_solver.h"
#include "plugin/product/orthogrid3d/detail/orthogonality.h"

namespace plugin {
namespace detail {
namespace {

geo::Raw3DPoint trilinear(const geo::Raw3DPoint c[8],
                          double u,
                          double v,
                          double w) {
  // Corner layout: 0(000) 1(100) 2(110) 3(010) 4(001) 5(101) 6(111) 7(011).
  const double ou = 1.0 - u;
  const double ov = 1.0 - v;
  const double ow = 1.0 - w;
  geo::Raw3DPoint p;
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

HexCornerSolve solve_hex_from_corners(const geo::Raw3DPoint corners[8],
                                      int nx,
                                      int ny,
                                      int nz) {
  HexCornerSolve out;
  if (corners == nullptr || nx < 3 || ny < 3 || nz < 3) {
    out.message = "{\"error\":\"bad_dims\"}";
    return out;
  }

  out.grid.set_size(nx, ny, nz);
  const int n = out.grid.node_count();
  std::vector<double> xs(static_cast<size_t>(n));
  std::vector<double> ys(static_cast<size_t>(n));
  std::vector<double> zs(static_cast<size_t>(n));
  std::vector<std::uint8_t> unknown(static_cast<size_t>(n), 0);

  for (int k = 0; k < nz; ++k) {
    const double w = static_cast<double>(k) / static_cast<double>(nz - 1);
    for (int j = 0; j < ny; ++j) {
      const double v = static_cast<double>(j) / static_cast<double>(ny - 1);
      for (int i = 0; i < nx; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(nx - 1);
        const geo::Raw3DPoint p = trilinear(corners, u, v, w);
        const int idx = out.grid.index_of(i, j, k);
        xs[static_cast<size_t>(idx)] = p.x;
        ys[static_cast<size_t>(idx)] = p.y;
        zs[static_cast<size_t>(idx)] = p.z;
        const bool on_face = (i == 0 || i == nx - 1 || j == 0 || j == ny - 1 ||
                              k == 0 || k == nz - 1);
        unknown[static_cast<size_t>(idx)] = on_face ? 0 : 1;
      }
    }
  }

  orthogrid3d::VolumeField field;
  field.nx = nx;
  field.ny = ny;
  field.nz = nz;
  field.x = xs.data();
  field.y = ys.data();
  field.z = zs.data();
  if (!orthogrid3d::solve_laplace_3d(field, unknown.data())) {
    out.message = "{\"error\":\"laplace_failed\"}";
    return out;
  }

  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const int idx = out.grid.index_of(i, j, k);
        out.grid.set_node(
            i, j, k,
            geo::Raw3DPoint(xs[static_cast<size_t>(idx)],
                            ys[static_cast<size_t>(idx)],
                            zs[static_cast<size_t>(idx)]));
      }
    }
  }

  const orthogrid3d::OrthogonalityField3d orth =
      orthogrid3d::compute_orthogonality_3d(field);
  out.cell_orth = orth.cell_delta;
  out.ok = true;
  out.message = "{\"ok\":true,\"op\":\"orthogrid3d.create_hex_grid\",\"nx\":" +
                std::to_string(nx) + ",\"ny\":" + std::to_string(ny) +
                ",\"nz\":" + std::to_string(nz) + "}";
  return out;
}

}  // namespace detail
}  // namespace plugin
