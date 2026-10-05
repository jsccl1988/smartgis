// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>

#include "ogrsf_frmts.h"
#include "vista/mesh/tessellate.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using vista::FillTessOptions;
  using vista::LineCap;
  using vista::LineJoin;
  using vista::LineTessOptions;
  using vista::TessMesh;

  // Empty / invalid geometry → false.
  TessMesh empty_mesh;
  expect(!vista::tessellate_line(nullptr, empty_mesh), "null line");
  OGRLineString empty_line;
  expect(!vista::tessellate_line(&empty_line, empty_mesh), "0-pt line");
  empty_line.addPoint(1, 1);
  expect(!vista::tessellate_line(&empty_line, empty_mesh), "1-pt line");

  LineTessOptions opts;
  opts.half_width = 1.0;
  expect(!vista::tessellate_line(nullptr, opts, empty_mesh),
         "styled null");
  expect(!vista::tessellate_polyline(nullptr, 0, 2, opts, empty_mesh),
         "polyline null");

  // Simple segment: width > 0 → one quad (4 verts / 6 indices) for butt.
  OGRLineString seg;
  seg.addPoint(0, 0);
  seg.addPoint(10, 0);
  TessMesh solid;
  opts.half_width = 0.5;
  opts.cap = LineCap::kButt;
  opts.join = LineJoin::kMiter;
  expect(vista::tessellate_line(&seg, opts, solid), "solid segment");
  expect(solid.positions.size() == 12, "butt segment 4 xyz verts");
  expect(solid.indices.size() == 6, "butt segment 2 tris");

  // Pixel width + resolution → same half-width as world 0.5.
  LineTessOptions px;
  px.pixel_width = 2.0;
  px.world_units_per_pixel = 0.5;  // half_width = 0.5
  px.cap = LineCap::kButt;
  TessMesh from_px;
  expect(vista::tessellate_line(&seg, px, from_px), "pixel width");
  expect(from_px.indices.size() == solid.indices.size(),
         "pixel path index count");
  expect(vista::line_half_width_world(2.0, 0.5) == 0.5, "half world");
  expect(vista::line_half_width_from_envelope(2.0, 100.0, 200.0) == 0.5,
         "half from envelope");

  // Polyline with a corner: more indices than a single segment.
  OGRLineString corner;
  corner.addPoint(0, 0);
  corner.addPoint(10, 0);
  corner.addPoint(10, 10);
  TessMesh corner_mesh;
  opts.half_width = 1.0;
  opts.join = LineJoin::kBevel;
  expect(vista::tessellate_line(&corner, opts, corner_mesh), "corner");
  expect(corner_mesh.indices.size() > 12, "corner has segment+join tris");
  expect(corner_mesh.positions.size() / 3 >= 8, "corner verts >= 8");

  // Dash pattern splits a long segment into multiple ribbons.
  LineTessOptions dashed = opts;
  dashed.half_width = 0.5;
  dashed.cap = LineCap::kButt;
  dashed.join = LineJoin::kMiter;
  dashed.dasharray = {2.0, 2.0};  // on 2, off 2 (world units)
  TessMesh dash_mesh;
  expect(vista::tessellate_line(&seg, dashed, dash_mesh), "dash line");
  // Length 10 → dashes at [0-2],[4-6],[8-10] = 3 quads minimum.
  expect(dash_mesh.indices.size() >= 18, "dash yields multiple quads");
  expect(dash_mesh.indices.size() > solid.indices.size(),
         "dash has more indices than solid same segment");

  // Round caps add fan triangles beyond the butt quad.
  LineTessOptions round_caps;
  round_caps.half_width = 0.5;
  round_caps.cap = LineCap::kRound;
  round_caps.round_segments = 4;
  TessMesh round_mesh;
  expect(vista::tessellate_line(&seg, round_caps, round_mesh),
         "round caps");
  expect(round_mesh.indices.size() > solid.indices.size(),
         "round caps add fan tris");

  // Legacy overload still matches historical one-quad segment.
  TessMesh legacy;
  expect(vista::tessellate_line(&seg, legacy), "legacy line");
  expect(legacy.indices.size() == 6, "legacy one quad");

  // Concave C-ring: ear-clip must fill the bay instead of a fan chord.
  OGRLinearRing concave;
  concave.addPoint(0, 0);
  concave.addPoint(4, 0);
  concave.addPoint(4, 3);
  concave.addPoint(2, 1);  // bay
  concave.addPoint(0, 3);
  concave.closeRings();
  OGRPolygon concave_poly;
  concave_poly.addRing(&concave);
  TessMesh concave_mesh;
  expect(vista::tessellate_geometry(&concave_poly, concave_mesh), "concave fill");
  expect(concave_mesh.indices.size() >= 9, "concave >= 3 tris");
  // Centroid of the bay triangle (2,1)-(4,3)-(0,3) is outside; ear-clip
  // must still produce coverage near (1,0.5) inside the C.
  expect(!concave_mesh.indices.empty(), "concave has indices");

  // Deep C where vertex-average of the whole ring sits in the bay (outside).
  // Stall-fallback must still seed an interior point and cover the arms.
  OGRLinearRing deep_c;
  deep_c.addPoint(0, 0);
  deep_c.addPoint(10, 0);
  deep_c.addPoint(10, 2);
  deep_c.addPoint(3, 2);
  deep_c.addPoint(3, 8);
  deep_c.addPoint(10, 8);
  deep_c.addPoint(10, 10);
  deep_c.addPoint(0, 10);
  deep_c.closeRings();
  OGRPolygon deep_poly;
  deep_poly.addRing(&deep_c);
  TessMesh deep_mesh;
  expect(vista::tessellate_geometry(&deep_poly, deep_mesh), "deep-C fill");
  expect(deep_mesh.indices.size() >= 18, "deep-C has arm coverage");

  // Dense near-collinear frontier (Mongolia bite): inclusive PIP used to
  // stall ear-clip and drop the remainder → ocean holes under labels.
  OGRLinearRing dense;
  dense.addPoint(0, 0);
  dense.addPoint(10, 0);
  dense.addPoint(10, 6);
  dense.addPoint(7, 6);
  dense.addPoint(7, 3);
  dense.addPoint(6.5, 3.02);
  dense.addPoint(6, 3);
  dense.addPoint(5.5, 3.01);
  dense.addPoint(5, 3);
  dense.addPoint(3, 6);
  dense.addPoint(0, 6);
  dense.closeRings();
  OGRPolygon dense_poly;
  dense_poly.addRing(&dense);
  FillTessOptions dense_opts;
  dense_opts.world_units_per_pixel = 0.05;
  dense_opts.max_fan_verts = 64;
  TessMesh dense_mesh;
  expect(vista::tessellate_geometry(&dense_poly, dense_opts, dense_mesh),
         "dense concave fill");
  expect(dense_mesh.indices.size() >= 15, "dense concave keeps coverage");

  // Grid quads store verts around the ring. Strip indices (0,1,2)/(1,3,2)
  // leave a hole on the v0–v3 half; ring indices (0,1,2)/(0,2,3) cover it.
  auto xyz = [](const TessMesh& m, uint32_t i, float* x, float* y) {
    *x = m.positions[static_cast<size_t>(i) * 3];
    *y = m.positions[static_cast<size_t>(i) * 3 + 1];
  };
  auto same_sign = [](float a, float b, float c) {
    return (a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0);
  };
  auto in_tri = [&](float px, float py, float ax, float ay, float bx, float by,
                    float cx, float cy) {
    const float c0 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    const float c1 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
    const float c2 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
    return same_sign(c0, c1, c2);
  };
  auto mesh_covers = [&](const TessMesh& m, float px, float py) {
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
      float ax = 0;
      float ay = 0;
      float bx = 0;
      float by = 0;
      float cx = 0;
      float cy = 0;
      xyz(m, m.indices[i], &ax, &ay);
      xyz(m, m.indices[i + 1], &bx, &by);
      xyz(m, m.indices[i + 2], &cx, &cy);
      if (in_tri(px, py, ax, ay, bx, by, cx, cy)) {
        return true;
      }
    }
    return false;
  };

  OGRMultiPoint grid_mp;
  OGRPoint gp00(0, 0);
  OGRPoint gp10(1, 0);
  OGRPoint gp01(0, 1);
  OGRPoint gp11(1, 1);
  grid_mp.addGeometry(&gp00);
  grid_mp.addGeometry(&gp10);
  grid_mp.addGeometry(&gp01);
  grid_mp.addGeometry(&gp11);
  TessMesh grid_mesh;
  expect(vista::tessellate_grid(&grid_mp, 2, 2, grid_mesh), "unit grid");
  expect(grid_mesh.indices.size() == 6, "unit grid two tris");
  expect(mesh_covers(grid_mesh, 0.25f, 0.6f), "grid covers v0-v3 half");
  expect(mesh_covers(grid_mesh, 0.75f, 0.4f), "grid covers v1-v2 half");

  if (g_fails) {
    std::fprintf(stderr, "tessellate_style_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "tessellate_style_test: ok\n");
  return 0;
}
