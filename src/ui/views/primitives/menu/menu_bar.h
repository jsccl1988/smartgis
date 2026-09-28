// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_MENU_MENU_BAR_H_
#define UI_VIEWS_PRIMITIVES_MENU_MENU_BAR_H_

#include "ui/ui_views_export.h"
#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"
#include "ui/views/primitives/menu/context_menu.h"

namespace ui {
namespace views {

// Top shell strip of utf8 labels. add_item runs a callback. add_menu stores
// child rows and opens show_context_menu under that top item when activated.
// Alt focuses the bar; Left/Right moves hover.
class UI_VIEWS_EXPORT MenuBar : public View {
 public:
  using Invoke = std::function<void()>;

  MenuBar();

  void add_item(std::string label, Invoke invoke);
  // Dropdown. Activation opens |items| with show_context_menu under this label.
  void add_menu(std::string label, std::vector<MenuItem> items);
  void clear();
  size_t item_count() const { return items_.size(); }

  // Child rows stored for the top item at |index|. Empty when |index| is out
  // of range or that slot was created with add_item.
  const std::vector<MenuItem>& menu_items(size_t index) const;

  // Index of the dropdown last activated, or -1 when none has opened.
  int last_opened_menu() const { return last_opened_menu_; }

  bool on_mouse_event(const MouseEvent& event) override;
  bool on_key_event(const KeyEvent& event) override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  struct Item {
    std::string label;
    Invoke invoke;
    std::vector<MenuItem> menu;
    bool dropdown = false;
  };

  int item_width(size_t i) const;
  int item_at(int x, int y) const;
  Rect item_rect(size_t i) const;
  void activate(int i);
  void open_dropdown(size_t index);

  std::vector<Item> items_;
  int hover_ = -1;
  int last_opened_menu_ = -1;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_MENU_MENU_BAR_H_
