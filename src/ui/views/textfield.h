// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_TEXTFIELD_H_
#define UI_VIEWS_TEXTFIELD_H_

#include <functional>
#include <string>

#include "ui/views/view.h"

namespace ui {
namespace views {

// Single-line text field. Accepts printable characters and backspace when
// focused. set_change fires after the text actually changes.
class Textfield : public View {
 public:
  Textfield();
  void set_text(std::string text);
  const std::string& text() const;
  void set_change(std::function<void()> fn);
  bool on_mouse_event(const MouseEvent& e) override;
  bool on_key_event(const KeyEvent& e) override;
  bool on_char_event(const CharEvent& e) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  void notify_change();
  bool delete_last_char();

  std::string text_;
  std::function<void()> change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_TEXTFIELD_H_
