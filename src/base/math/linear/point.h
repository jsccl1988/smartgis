// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_POINT_H_
#define SMT_RENDER_MATH_POINT_H_

#include "base/math/scalar/constants.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <type_traits>

namespace base {
namespace detail {

template <typename T>
inline bool coord_equal(T a, T b) {
  if constexpr (std::is_floating_point_v<T>) {
    return std::fabs(static_cast<double>(a) - static_cast<double>(b)) <
           static_cast<double>(kEpsilon);
  } else {
    return a == b;
  }
}

}  // namespace detail

// 2D point. Integer specializations are device pixels; float/double are map LP.
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

// Axis-aligned 2D box with inclusive lb/rt (GDI LP/DP). Map extents use
// gis::Envelope instead.
template <typename T>
struct Rect {
  using coordinate_type = T;
  using point_type = Point2<T>;

  point_type lb{};
  point_type rt{};

  void merge(T x, T y) {
    const point_type origin{};
    if (lb == origin && rt == origin) {
      lb = rt = point_type(x, y);
    } else {
      lb.x = (std::min)(lb.x, x);
      rt.x = (std::max)(rt.x, x);
      lb.y = (std::min)(lb.y, y);
      rt.y = (std::max)(rt.y, y);
    }
  }

  void normalize() {
    const point_type a = lb;
    const point_type b = rt;
    lb.x = (std::min)(a.x, b.x);
    lb.y = (std::min)(a.y, b.y);
    rt.x = (std::max)(a.x, b.x);
    rt.y = (std::max)(a.y, b.y);
  }

  bool contains(T x, T y) const {
    return lb.x <= x && lb.y <= y && rt.x >= x && rt.y >= y;
  }

  template <typename U>
  Rect<U> cast_to() const {
    auto conv = [](T v) -> U {
      if constexpr (std::is_floating_point_v<T> && std::is_integral_v<U>) {
        return static_cast<U>(std::lround(static_cast<double>(v)));
      } else {
        return static_cast<U>(v);
      }
    };
    return Rect<U>{Point2<U>(conv(lb.x), conv(lb.y)),
                   Point2<U>(conv(rt.x), conv(rt.y))};
  }

  T height() const {
    if constexpr (std::is_floating_point_v<T>) {
      return static_cast<T>(std::fabs(rt.y - lb.y));
    } else {
      return static_cast<T>(std::abs(rt.y - lb.y));
    }
  }

  T width() const {
    if constexpr (std::is_floating_point_v<T>) {
      return static_cast<T>(std::fabs(rt.x - lb.x));
    } else {
      return static_cast<T>(std::abs(rt.x - lb.x));
    }
  }
};

using lPoint = Point2<long>;
using fPoint = Point2<float>;
using dbfPoint = Point2<double>;

using l3DPoint = Point3<long>;
using f3DPoint = Point3<float>;
using dbf3DPoint = Point3<double>;

using lRect = Rect<long>;
using fRect = Rect<float>;
using dbfRect = Rect<double>;

}  // namespace base

#endif  // SMT_RENDER_MATH_POINT_H_
