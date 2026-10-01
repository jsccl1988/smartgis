// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_GDI_DRAW_MESH_H_
#define SMT_LEGACY_RENDER_GDI_DRAW_MESH_H_

#include "gis/kernel/geo/mesh/geometry.h"

namespace render {
namespace detail {

class Rhi2dCartoDraw;

// TIN / grid immediate draws for Rhi2dCartoDraw.
class GdiMeshDraw {
 public:
  explicit GdiMeshDraw(Rhi2dCartoDraw* carto_draw) : c_(carto_draw) {}

  int draw_tin(const geo::SmtTin* tin);
  int draw_tin_lines(const geo::SmtTin* tin);
  int draw_tin_nodes(const geo::SmtTin* tin);
  int draw_grid(const geo::SmtGrid* grid);
  int draw_grid_lines(const geo::SmtGrid* grid);
  int draw_grid_nodes(const geo::SmtGrid* grid);

 private:
  Rhi2dCartoDraw* c_;
};

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_GDI_DRAW_MESH_H_
