// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_TERRAIN_MESH_MESH_TYPES_H_
#define GIS_VISTA_WORLD_TERRAIN_MESH_MESH_TYPES_H_

#include <cmath>

namespace vista {
namespace detail {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;

// 2D direction / offset in world CRS units.
struct Vec2 {
  double x = 0;
  double y = 0;
};

// Polyline / ring sample with optional Z.
struct PolyPt {
  double x = 0;
  double y = 0;
  double z = 0;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, double s) { return {a.x * s, a.y * s}; }

inline double vec_length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

inline Vec2 vec_normalize(Vec2 v) {
  const double len = vec_length(v);
  if (len < kEps) {
    return {0, 0};
  }
  return {v.x / len, v.y / len};
}

// Left-hand unit normal for a direction (CCW).
inline Vec2 vec_perp(Vec2 v) { return {-v.y, v.x}; }

inline double vec_dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

inline double vec_cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_WORLD_TERRAIN_MESH_MESH_TYPES_H_
