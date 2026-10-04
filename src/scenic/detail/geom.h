// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_GEOM_H_
#define SCENIC_DETAIL_GEOM_H_

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdlib>
#include <type_traits>

#include "scenic/detail/err.h"

// 2D/3D point + AABB rect used by scenic rhi/scene3d. Lives in namespace base
// to match historical `using namespace base` call sites without legacy includes.

using uchar = unsigned char;
using ushort = unsigned short;
using ulong = unsigned long;
using uint = unsigned int;
using byte = unsigned char;

#ifndef TEMP_BUFFER_SIZE
#define TEMP_BUFFER_SIZE 255
#endif

namespace base {

namespace detail {

template <typename T>
inline bool coord_equal(T a, T b) {
  if constexpr (std::is_floating_point_v<T>) {
    return EQUAL(a, b);
  } else {
    return a == b;
  }
}

}  // namespace detail

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

  // Convert coordinate type (float→integral uses lround).
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

struct Triangle {
  long a = -1;
  long b = -1;
  long c = -1;
  bool bDelete = false;
};

using Triangle = Triangle;  // transitional
using Triangle = Triangle;

}  // namespace base

#endif  // SCENIC_DETAIL_GEOM_H_
