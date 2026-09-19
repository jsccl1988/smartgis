// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_LAYOUT_H_
#define UI_VIEWS_KERNEL_LAYOUT_H_

#include <map>

namespace ui {
namespace views {

class View;

// Positions children of a host View.
class LayoutManager {
 public:
  virtual ~LayoutManager() = default;
  virtual void layout(View* host) = 0;
};

// Single child fills the host; extra children are stacked on the same rect.
class FillLayout : public LayoutManager {
 public:
  void layout(View* host) override;
};

// Horizontal or vertical strip. Preferred size on the main axis wins unless
// flex > 0, in which case leftover space is shared.
class BoxLayout : public LayoutManager {
 public:
  enum class Orientation {
    kHorizontal,
    kVertical,
  };

  explicit BoxLayout(Orientation orientation);

  void set_flex_for_view(const View* view, int flex);
  // Chromium-style visual insets: padding inside the host and gap between
  // consecutive visible children (physical pixels; callers DIP-scale first).
  void set_inside_border(int inset_px);
  void set_inside_border(int left, int top, int right, int bottom);
  void set_between_child_spacing(int spacing_px);
  void layout(View* host) override;

 private:
  Orientation orientation_;
  std::map<const View*, int> flex_;
  int inset_left_ = 0;
  int inset_top_ = 0;
  int inset_right_ = 0;
  int inset_bottom_ = 0;
  int between_child_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_LAYOUT_H_
