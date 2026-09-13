// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_LAYOUT_H_
#define UI_VIEWS_LAYOUT_H_

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
  void layout(View* host) override;

 private:
  Orientation orientation_;
  std::map<const View*, int> flex_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_LAYOUT_H_
