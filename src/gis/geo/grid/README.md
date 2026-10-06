<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# grid（`src/gis/geo/grid`）

Structured-lattice elliptic smoothers and orthogonality metrics on `NodeField2d` / `NodeField3d`. Sibling of `tin/` (unstructured Delaunay), not folded into it. Public API: `geo::solve_laplace` (2D 5-point / 3D 7-point), `geo::solve_elliptic` (2D Thompson, P=Q=0), `geo::compute_orthogonality` (2D node+cell |90−θ|, 3D cell skew), and `geo::sample_orthogonality_raster` (axis-aligned bilinear heat samples). No `namespace grid`. XY lattice buffer is **not** here: product plugin `plugin::detail::OrthoLattice` (`src/plugin/product/world3d/scene/orthogrid/lattice/ortho_lattice.h`).

**Not in this tree:** TFI, clustering control functions P/Q, Thomas–Middlecoff, 3D Thompson. Orthogrid plugins keep boundary digitize / VTK / MapScene writers and call these ops.

GN：`//src/gis/geo:grid` → `//src/gis:gis`。测试：`geo_grid_laplace_test`。
