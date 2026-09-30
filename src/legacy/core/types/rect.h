// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SMT_LEGACY_CORE_TYPES_RECT_H
#define SMT_LEGACY_CORE_TYPES_RECT_H

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <type_traits>

#include "legacy/core/types/point.h"

namespace base {

// Axis-aligned rect over Point2<T> (lb = lower-left, rt = upper-right).
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

  // Ensure lb is the lower-left and rt the upper-right corner.
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

template <typename R>
struct rect_traits {
  using coordinate_type = typename R::coordinate_type;
  using point_type = typename R::point_type;

  static point_type& lb(R& r) { return r.lb; }
  static const point_type& lb(const R& r) { return r.lb; }
  static point_type& rt(R& r) { return r.rt; }
  static const point_type& rt(const R& r) { return r.rt; }
};

template <typename R>
concept rect_like = requires(const R& r) {
  typename rect_traits<R>::coordinate_type;
  typename rect_traits<R>::point_type;
  rect_traits<R>::lb(r);
  rect_traits<R>::rt(r);
};

using lRect = Rect<long>;
using fRect = Rect<float>;
using dbfRect = Rect<double>;

}  // namespace base

#endif  // SMT_LEGACY_CORE_TYPES_RECT_H
