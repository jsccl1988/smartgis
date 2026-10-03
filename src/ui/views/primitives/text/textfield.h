// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_TEXT_TEXTFIELD_H_
#define UI_VIEWS_PRIMITIVES_TEXT_TEXTFIELD_H_

#include "ui/ui_export.h"
#include <functional>
#include <string>
#include <string_view>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Single-line text field with caret blink, selection, clipboard shortcuts,
// and Win32 IME composition when the host Widget routes WM_IME_*.
class UI_EXPORT Textfield : public View {
 public:
  Textfield();
  ~Textfield() override;

  void set_text(std::string text);
  const std::string& text() const;
  // Shown in muted ink when |text| is empty (does not affect text()).
  void set_placeholder(std::string placeholder);
  // When true, paints a danger border (validation feedback).
  void set_invalid(bool invalid);
  bool is_invalid() const { return invalid_; }
  void set_change(std::function<void()> fn);
  // Fired on Enter (VK_RETURN) when focused.
  void set_submit(std::function<void()> fn);
  // Optional pre-handler for key downs (history / Tab). Return true if handled.
  void set_key_hook(std::function<bool(const KeyEvent&)> fn);
  bool on_mouse_event(const MouseEvent& e) override;
  bool on_key_event(const KeyEvent& e) override;
  bool on_char_event(const CharEvent& e) override;
  void on_focus() override;
  void on_blur() override;
  bool on_ime_composition(std::wstring_view text, bool is_result) override;
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void notify_change();
  void set_caret(size_t utf8_offset, bool extend_selection);
  void clear_selection();
  bool has_selection() const;
  size_t selection_begin() const;
  size_t selection_end() const;
  void delete_selection();
  void insert_utf8(std::string_view utf8);
  bool delete_backward();
  bool delete_forward();
  void move_caret(int utf8_delta, bool extend_selection);
  void select_all();
  bool copy_selection_to_clipboard() const;
  bool cut_selection_to_clipboard();
  bool paste_from_clipboard();
  size_t caret_from_x(int client_x) const;
  void arm_caret_timer();
  void disarm_caret_timer();

  std::string text_;
  std::string placeholder_;
  std::wstring composition_;
  size_t caret_ = 0;
  size_t sel_anchor_ = 0;
  bool invalid_ = false;
  bool selecting_drag_ = false;
  std::function<void()> change_;
  std::function<void()> submit_;
  std::function<bool(const KeyEvent&)> key_hook_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_TEXT_TEXTFIELD_H_
