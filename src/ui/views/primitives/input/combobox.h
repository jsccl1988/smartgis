// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_INPUT_COMBOBOX_H_
#define UI_VIEWS_PRIMITIVES_INPUT_COMBOBOX_H_

#include "ui/ui_export.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Widget;

// String list dropdown. Opens a borderless owned Widget popup below the
// header (does not expand parent layout). Up/Down cycles when closed; when
// open, keys route to the popup list. Esc / deactivate / outside click close.
class UI_EXPORT Combobox : public View {
 public:
  Combobox();
  ~Combobox() override;

  void add_item(std::string item);
  void clear_items();
  void set_selected_index(int i);
  int selected_index() const;
  const std::string& selected_text() const;
  int item_count() const { return static_cast<int>(items_.size()); }
  bool is_open() const { return open_; }
  void set_change(std::function<void(int)> fn);
  bool on_mouse_event(const MouseEvent& e) override;
  bool on_key_event(const KeyEvent& e) override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  class DropdownList;

  void set_open(bool open);
  void select_item(int index);
  void cycle(int delta);
  void show_popup();
  void hide_popup();
  void flush_closed_popup();
  int header_height() const;
  int row_height() const;
  float scale_factor() const;

  std::vector<std::string> items_;
  int selected_ = -1;
  int hover_index_ = -1;
  std::string empty_;
  bool open_ = false;
  std::function<void(int)> change_;
  std::unique_ptr<Widget> popup_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_INPUT_COMBOBOX_H_
