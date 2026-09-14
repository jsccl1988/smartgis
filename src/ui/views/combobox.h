// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_COMBOBOX_H_
#define UI_VIEWS_COMBOBOX_H_

#include <functional>
#include <string>
#include <vector>

#include "ui/views/view.h"

namespace ui {
namespace views {

// String list dropdown. Click toggles a child item list; Up/Down cycles
// when focused. Not a native HWND combo.
class Combobox : public View {
 public:
  Combobox();
  void add_item(std::string item);
  void set_selected_index(int i);
  int selected_index() const;
  const std::string& selected_text() const;
  int item_count() const { return static_cast<int>(items_.size()); }
  bool is_open() const { return open_; }
  void set_change(std::function<void(int)> fn);
  bool on_mouse_event(const MouseEvent& e) override;
  bool on_key_event(const KeyEvent& e) override;
  void layout() override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  class ItemRow;

  void set_open(bool open);
  void select_item(int index);
  void cycle(int delta);
  void rebuild_rows();
  // DIP metrics scaled by the host Widget (defaults to 1.0 without a widget).
  int header_height() const;
  int row_height() const;
  float scale_factor() const;

  std::vector<std::string> items_;
  int selected_ = -1;
  std::string empty_;
  bool open_ = false;
  std::function<void(int)> change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_COMBOBOX_H_
