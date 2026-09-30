// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SMT_LEGACY_CORE_TYPES_POINT_H
#define SMT_LEGACY_CORE_TYPES_POINT_H

#include <cmath>
#include <concepts>
#include <type_traits>

#include "legacy/core/macros/macros.h"

namespace base {

namespace detail {

template <typename T>
inline bool coord_equal(T a, T b) {
  if constexpr (std::is_floating_point_v<T>) {
    return SMT_EQUAL(a, b);
  } else {
    return a == b;
  }
}

}  // namespace detail

// 2D point parameterized by coordinate type.
template <typename T>
struct Point2 {
  using coordinate_type = T;
  static constexpr int dimension = 2;

  T x{};
  T y{};

  Point2() = default;
  Point2(T x_in, T y_in) : x(x_in), y(y_in) {}

  bool operator==(const Point2& o) const {
    return detail::coord_equal(x, o.x) && detail::coord_equal(y, o.y);
  }
  bool operator!=(const Point2& o) const { return !(*this == o); }
};

// 3D point parameterized by coordinate type.
template <typename T>
struct Point3 {
  using coordinate_type = T;
  static constexpr int dimension = 3;

  T x{};
  T y{};
  T z{};

  Point3() = default;
  Point3(T x_in, T y_in, T z_in) : x(x_in), y(y_in), z(z_in) {}

  bool operator==(const Point3& o) const {
    return detail::coord_equal(x, o.x) && detail::coord_equal(y, o.y) &&
           detail::coord_equal(z, o.z);
  }
  bool operator!=(const Point3& o) const { return !(*this == o); }
};

// Traits for Point2 / Point3 (and any type exposing the same surface).
template <typename P>
struct point_traits {
  using coordinate_type = typename P::coordinate_type;
  static constexpr int dimension = P::dimension;

  static coordinate_type& x(P& p) { return p.x; }
  static const coordinate_type& x(const P& p) { return p.x; }
  static coordinate_type& y(P& p) { return p.y; }
  static const coordinate_type& y(const P& p) { return p.y; }

  static coordinate_type& z(P& p)
    requires(dimension == 3)
  {
    return p.z;
  }
  static const coordinate_type& z(const P& p)
    requires(dimension == 3)
  {
    return p.z;
  }
};

template <typename P>
concept point_like = requires(const P& p) {
  typename point_traits<P>::coordinate_type;
  { point_traits<P>::dimension } -> std::convertible_to<int>;
  point_traits<P>::x(p);
  point_traits<P>::y(p);
};

using lPoint = Point2<long>;
using fPoint = Point2<float>;
using dbfPoint = Point2<double>;

using l3DPoint = Point3<long>;
using f3DPoint = Point3<float>;
using dbf3DPoint = Point3<double>;

}  // namespace base

#endif  // SMT_LEGACY_CORE_TYPES_POINT_H
