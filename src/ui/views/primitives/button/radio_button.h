// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_BUTTON_RADIO_BUTTON_H_
#define UI_VIEWS_PRIMITIVES_BUTTON_RADIO_BUTTON_H_

#include "ui/ui_export.h"
#include <functional>
#include <string>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Exclusive radio option. group_id is unique among RadioButtons in the same
// widget tree (not only immediate siblings — markup often wraps each option
// in a row).
class UI_EXPORT RadioButton : public View {
 public:
  RadioButton(std::string label, int group_id);
  void set_selected(bool on);
  bool is_selected() const;
  int group_id() const;
  void set_label(std::string label);
  const std::string& label() const;
  void set_change(std::function<void()> fn);
  bool on_mouse_event(const MouseEvent& e) override;
  bool on_key_event(const KeyEvent& e) override;
  void layout() override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void select_from_user();
  void rebuild_preferred_size();
  void exclusive_unselect_peers();

  std::string label_;
  int group_id_ = 0;
  bool selected_ = false;
  std::function<void()> change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_BUTTON_RADIO_BUTTON_H_
