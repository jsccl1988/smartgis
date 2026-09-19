// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_CHECKBOX_H_
#define UI_VIEWS_PRIMITIVES_CHECKBOX_H_

#include <functional>
#include <string>

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

// Boolean checkbox with a label. Toggle on click or Space when focused.
class Checkbox : public View {
 public:
  explicit Checkbox(std::string label);
  void set_checked(bool on);
  bool is_checked() const;
  void set_label(std::string label);
  const std::string& label() const;
  void set_change(std::function<void(bool)> fn);
  bool on_mouse_event(const MouseEvent& e) override;
  bool on_key_event(const KeyEvent& e) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  void toggle();

  std::string label_;
  bool checked_ = false;
  std::function<void(bool)> change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_CHECKBOX_H_
