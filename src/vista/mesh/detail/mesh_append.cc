// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/mesh/detail/mesh_append.h"

namespace vista {
namespace detail {

void reset_mesh(TessMesh& out) {
  out.positions.clear();
  out.indices.clear();
  out.has_image = false;
}

void append_xyz(TessMesh& mesh, float x, float y, float z) {
  mesh.positions.push_back(x);
  mesh.positions.push_back(y);
  mesh.positions.push_back(z);
}

void append_xyz(TessMesh& mesh, Vec2 p, double z) {
  append_xyz(mesh, static_cast<float>(p.x), static_cast<float>(p.y),
             static_cast<float>(z));
}

uint32_t vert_count(const TessMesh& mesh) {
  return static_cast<uint32_t>(mesh.positions.size() / 3);
}

void append_triangle(TessMesh& out, uint32_t a, uint32_t b, uint32_t c) {
  out.indices.push_back(a);
  out.indices.push_back(b);
  out.indices.push_back(c);
}

void append_quad_indices(TessMesh& out, uint32_t base) {
  append_triangle(out, base, base + 1, base + 2);
  append_triangle(out, base + 1, base + 3, base + 2);
}

void append_quad(double min_x, double min_y, double max_x, double max_y,
                 TessMesh& out) {
  const uint32_t base = vert_count(out);
  append_xyz(out, static_cast<float>(min_x), static_cast<float>(min_y), 0);
  append_xyz(out, static_cast<float>(max_x), static_cast<float>(min_y), 0);
  append_xyz(out, static_cast<float>(max_x), static_cast<float>(max_y), 0);
  append_xyz(out, static_cast<float>(min_x), static_cast<float>(max_y), 0);
  append_quad_indices(out, base);
}

bool rect_has_area(double min_x, double min_y, double max_x, double max_y) {
  return max_x > min_x && max_y > min_y;
}

}  // namespace detail
}  // namespace vista
