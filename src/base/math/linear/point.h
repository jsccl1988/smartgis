// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MATH_LINEAR_POINT_H_
#define BASE_MATH_LINEAR_POINT_H_

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

using Point2l = Point2<long>;
using Point2f = Point2<float>;
using Point2d = Point2<double>;
using Point3l = Point3<long>;
using Point3f = Point3<float>;
using Point3d = Point3<double>;
using Rect2l = Rect<long>;
using Rect2f = Rect<float>;
using Rect2d = Rect<double>;

// Leftover GDI/carto spellings. New TUs use Point2f / Rect2f.
using lPoint = Point2l;
using fPoint = Point2f;
using dbfPoint = Point2d;
using l3DPoint = Point3l;
using f3DPoint = Point3f;
using dbf3DPoint = Point3d;
using lRect = Rect2l;
using fRect = Rect2f;
using dbfRect = Rect2d;

}  // namespace base

namespace render {
using ::base::Point2;
using ::base::Point3;
using ::base::Rect;
using ::base::Point2l;
using ::base::Point2f;
using ::base::Point2d;
using ::base::Point3l;
using ::base::Point3f;
using ::base::Point3d;
using ::base::Rect2l;
using ::base::Rect2f;
using ::base::Rect2d;
using ::base::lPoint;
using ::base::fPoint;
using ::base::dbfPoint;
using ::base::l3DPoint;
using ::base::f3DPoint;
using ::base::dbf3DPoint;
using ::base::lRect;
using ::base::fRect;
using ::base::dbfRect;
}  // namespace render

#endif  // BASE_MATH_LINEAR_POINT_H_
