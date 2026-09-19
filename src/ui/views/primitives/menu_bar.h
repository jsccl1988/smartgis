// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_MENU_BAR_H_
#define UI_VIEWS_PRIMITIVES_MENU_BAR_H_

#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

// Top chrome strip of utf8 labels. Click (or Enter/Space when focused)
// runs the item callback. Alt focuses the bar; Left/Right moves hover.
class MenuBar : public View {
 public:
  using Invoke = std::function<void()>;

  MenuBar();

  void add_item(std::string label, Invoke invoke);
  void clear();
  size_t item_count() const { return items_.size(); }

  bool on_mouse_event(const MouseEvent& event) override;
  bool on_key_event(const KeyEvent& event) override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  struct Item {
    std::string label;
    Invoke invoke;
  };

  int item_width(size_t i) const;
  int item_at(int x, int y) const;
  Rect item_rect(size_t i) const;
  void activate(int i);

  std::vector<Item> items_;
  int hover_ = -1;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_MENU_BAR_H_
