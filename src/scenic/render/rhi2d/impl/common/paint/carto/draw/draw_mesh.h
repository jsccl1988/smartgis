// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_GDI_DRAW_MESH_H_
#define SCENIC_GDI_DRAW_MESH_H_

#include "plugin/product/world3d/scene/orthogrid/lattice/ortho_lattice.h"

namespace scenic {
namespace detail {

class Rhi2dCartoDraw;

// TIN / grid immediate draws for Rhi2dCartoDraw.
class GdiMeshDraw {
 public:
  explicit GdiMeshDraw(Rhi2dCartoDraw* carto_draw) : c_(carto_draw) {}

  int draw_tin(const OGRTriangulatedSurface* tin);
  int draw_tin_lines(const OGRTriangulatedSurface* tin);
  int draw_tin_nodes(const OGRTriangulatedSurface* tin);
  int draw_grid(const plugin::detail::OrthoLattice* grid);
  int draw_grid_lines(const plugin::detail::OrthoLattice* grid);
  int draw_grid_nodes(const plugin::detail::OrthoLattice* grid);

 private:
  Rhi2dCartoDraw* c_;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_GDI_DRAW_MESH_H_
