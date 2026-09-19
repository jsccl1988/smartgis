// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_BUTTON_H_
#define UI_VIEWS_PRIMITIVES_BUTTON_H_

#include <functional>
#include <string>

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

// Clickable text button with hover / press / disabled paint and keyboard
// Activate (Space / Return when focused).
class Button : public View {
 public:
  explicit Button(std::string text);
  void set_text(std::string text);
  const std::string& text() const;
  void set_click(std::function<void()> fn);
  bool on_mouse_event(const MouseEvent& e) override;
  bool on_key_event(const KeyEvent& e) override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  void activate();

  std::string text_;
  std::function<void()> click_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_BUTTON_H_
