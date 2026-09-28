// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_LAYOUT_LAYOUT_H_
#define UI_VIEWS_KERNEL_LAYOUT_LAYOUT_H_

#include "ui/ui_export.h"
#include <utility>
#include <vector>

#include "ui/views/kernel/layout/view_traits.h"

namespace ui {
namespace views {

// Positions children of a host View and reports preferred host size.
class UI_EXPORT LayoutManager {
 public:
  virtual ~LayoutManager() = default;
  virtual void layout(View* host) = 0;
  // Preferred size of |host| from children. Default: stored preferred_size.
  virtual Size get_preferred_size(const View* host) const;
};

// Static dispatch into Derived. The vtable slot stays on the derived class
// (out of line) so the symbol is FillLayout::layout / BoxLayout::layout.
// View still holds LayoutManager*; a concrete pointer devirtualizes to that
// one slot, and the slot static_casts instead of a second virtual call.
template <typename Derived>
class LayoutCrtp : public LayoutManager {
 protected:
  void layout_static(View* host) {
    static_cast<Derived*>(this)->layout_impl(host);
  }

  Size preferred_static(const View* host) const {
    return static_cast<const Derived*>(this)->preferred_impl(host);
  }
};

// Single visible child fills the host. Extra visible children share the same
// rect (overlay); prefer one child for shell panels.
class UI_EXPORT FillLayout : public LayoutCrtp<FillLayout> {
 public:
  void layout(View* host) final;
  Size get_preferred_size(const View* host) const final;

  friend class LayoutCrtp<FillLayout>;

 private:
  void layout_impl(View* host);
  Size preferred_impl(const View* host) const;
};

// Horizontal or vertical strip.
//
// Main-axis sizing:
//   - Every visible child reserves its preferred size on the main axis.
//   - Positive leftover (host - sum preferred - spacing) is shared by flex > 0.
//   - Negative leftover shrinks flex children first (down to 0), then non-flex
//     proportionally so children stay inside the host (no stack / paint clip).
// Cross axis: stretch to the host's inner cross size.
// The per-child loop is a template on Axis (see layout.cc): one preferred-size
// query, no orientation branch, flex in a flat table.
class UI_EXPORT BoxLayout : public LayoutCrtp<BoxLayout> {
 public:
  enum class Orientation {
    kHorizontal,
    kVertical,
  };

  static_assert(static_cast<int>(Orientation::kHorizontal) ==
                static_cast<int>(Axis::kHorizontal));
  static_assert(static_cast<int>(Orientation::kVertical) ==
                static_cast<int>(Axis::kVertical));

  explicit BoxLayout(Orientation orientation);

  void set_flex_for_view(const View* view, int flex);
  // Visual insets: padding inside the host and gap between
  // consecutive visible children (physical pixels; callers DIP-scale first).
  void set_inside_border(int inset_px);
  void set_inside_border(int left, int top, int right, int bottom);
  void set_between_child_spacing(int spacing_px);
  void layout(View* host) final;
  Size get_preferred_size(const View* host) const final;

  friend class LayoutCrtp<BoxLayout>;

 private:
  template <Axis A>
  void layout_along(View* host);
  template <Axis A>
  Size preferred_along(const View* host) const;

  void layout_impl(View* host);
  Size preferred_impl(const View* host) const;
  int flex_of(const View* view) const;

  Orientation orientation_;
  std::vector<std::pair<const View*, int>> flex_;
  int inset_left_ = 0;
  int inset_top_ = 0;
  int inset_right_ = 0;
  int inset_bottom_ = 0;
  int between_child_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_LAYOUT_LAYOUT_H_
