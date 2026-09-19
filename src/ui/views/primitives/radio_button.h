// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_RADIO_BUTTON_H_
#define UI_VIEWS_PRIMITIVES_RADIO_BUTTON_H_

#include <functional>
#include <string>

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

// Exclusive radio option within a group_id among sibling RadioButtons.
class RadioButton : public View {
 public:
  RadioButton(std::string label, int group_id);
  void set_selected(bool on);
  bool is_selected() const;
  int group_id() const;
  const std::string& label() const;
  void set_change(std::function<void()> fn);
  bool on_mouse_event(const MouseEvent& e) override;
  bool on_key_event(const KeyEvent& e) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  void select_from_user();

  std::string label_;
  int group_id_ = 0;
  bool selected_ = false;
  std::function<void()> change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_RADIO_BUTTON_H_
