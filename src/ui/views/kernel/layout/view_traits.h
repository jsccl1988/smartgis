// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_LAYOUT_VIEW_TRAITS_H_
#define UI_VIEWS_KERNEL_LAYOUT_VIEW_TRAITS_H_

#include <concepts>
#include <cstddef>
#include <type_traits>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Compile-time main axis. BoxLayout and Splitter orientations use the same
// numeric order so they convert with a checked cast.
enum class Axis : int { kHorizontal = 0, kVertical = 1 };

template <typename E>
concept axis_enum = std::is_enum_v<E> && requires {
  E::kHorizontal;
  E::kVertical;
};

template <axis_enum E>
constexpr Axis to_axis(E orientation) {
  return static_cast<Axis>(static_cast<int>(orientation));
}

// Main/cross accessors. if constexpr drops the other axis from the layout
// loop, so a horizontal pass never tests "am I vertical?" per child.
template <Axis A>
struct axis_traits {
  static constexpr bool horizontal = A == Axis::kHorizontal;

  static constexpr int main(const Size& s) {
    if constexpr (horizontal) {
      return s.width;
    } else {
      return s.height;
    }
  }

  static constexpr int cross(const Size& s) {
    if constexpr (horizontal) {
      return s.height;
    } else {
      return s.width;
    }
  }

  static constexpr int main(const Rect& r) {
    if constexpr (horizontal) {
      return r.width;
    } else {
      return r.height;
    }
  }

  static constexpr int cross(const Rect& r) {
    if constexpr (horizontal) {
      return r.height;
    } else {
      return r.width;
    }
  }

  static constexpr Size make_size(int main_px, int cross_px) {
    if constexpr (horizontal) {
      return {main_px, cross_px};
    } else {
      return {cross_px, main_px};
    }
  }

  // Child rect. |cursor| is the main-axis offset inside the host inner box.
  static constexpr Rect place(int inner_x, int inner_y, int cursor, int main_px,
                              int cross_px) {
    if constexpr (horizontal) {
      return {inner_x + cursor, inner_y, main_px, cross_px};
    } else {
      return {inner_x, inner_y + cursor, cross_px, main_px};
    }
  }

  static Rect bar(const Rect& host, int primary, int bar_px) {
    if constexpr (horizontal) {
      return {host.x + primary, host.y, bar_px, host.height};
    } else {
      return {host.x, host.y + primary, host.width, bar_px};
    }
  }

  static Rect primary_pane(const Rect& host, int primary) {
    if constexpr (horizontal) {
      return {host.x, host.y, primary, host.height};
    } else {
      return {host.x, host.y, host.width, primary};
    }
  }

  static Rect secondary_pane(const Rect& host, int primary, int bar_px) {
    if constexpr (horizontal) {
      const int x = host.x + primary + bar_px;
      const int w = host.x + host.width - x;
      return {x, host.y, w < 0 ? 0 : w, host.height};
    } else {
      const int y = host.y + primary + bar_px;
      const int h = host.y + host.height - y;
      return {host.x, y, host.width, h < 0 ? 0 : h};
    }
  }
};

// Operations the layout kernel reads. The widget tree stays virtual View*:
// children are heterogeneous, so paint/hit-test cannot be a closed CRTP set.
template <typename V>
concept view_node = requires(const V& v, V& m, const Rect& r) {
  { v.child_count() } -> std::convertible_to<std::size_t>;
  { v.child_at(std::size_t{}) } -> std::convertible_to<const View*>;
  { m.child_at(std::size_t{}) } -> std::convertible_to<View*>;
  { v.bounds() } -> std::convertible_to<const Rect&>;
  { v.is_visible() } -> std::convertible_to<bool>;
  { v.is_locally_visible() } -> std::convertible_to<bool>;
  { v.get_preferred_size() } -> std::convertible_to<Size>;
  { v.preferred_size() } -> std::convertible_to<const Size&>;
  { m.set_bounds(r) };
};

template <typename V>
struct view_traits {
  static_assert(view_node<V>);

  static bool locally_visible(const V& v) { return v.is_locally_visible(); }

  // One preferred-size query. Callers split main/cross with axis_traits.
  static Size preferred(const V& v) { return v.get_preferred_size(); }

  static const Rect& bounds(const V& v) { return v.bounds(); }
};

template <typename F>
concept flex_lookup = requires(const F& f, const View* v) {
  { f(v) } -> std::convertible_to<int>;
};

static_assert(view_node<View>);
static_assert(axis_traits<Axis::kHorizontal>::main(Size{3, 5}) == 3);
static_assert(axis_traits<Axis::kVertical>::main(Size{3, 5}) == 5);
static_assert(axis_traits<Axis::kHorizontal>::cross(Size{3, 5}) == 5);
static_assert(axis_traits<Axis::kVertical>::place(1, 2, 3, 4, 5).y == 5);
static_assert(axis_traits<Axis::kVertical>::place(1, 2, 3, 4, 5).height == 4);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_LAYOUT_VIEW_TRAITS_H_
