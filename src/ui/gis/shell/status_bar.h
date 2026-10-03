// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_SHELL_STATUS_BAR_H_
#define UI_GIS_SHELL_STATUS_BAR_H_

#include "ui/ui_export.h"
#include <string>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Label;

// Bottom strip: scale, CRS, XY, and a flexible status message.
// The bar is focusable; Left/Right cycles the highlighted field (read-only).
class UI_EXPORT StatusBar : public View {
 public:
  StatusBar();

  void set_xy(double x, double y);
  void set_scale(double scale);
  void set_message(std::string text);

  void set_scale_text(std::string text);
  void set_crs_text(std::string text);
  void set_coord_text(std::string text);
  void set_status(std::string text);

  const std::string& scale_text() const;
  const std::string& crs_text() const;
  const std::string& coord_text() const;
  const std::string& status() const;
  const std::string& message() const { return status(); }

  bool on_key_event(const KeyEvent& event) override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;
  void on_focus() override;
  void on_blur() override;

 private:
  Label* field_at(int index) const;
  static constexpr int kFieldCount = 4;

  Label* scale_ = nullptr;
  Label* crs_ = nullptr;
  Label* coord_ = nullptr;
  Label* status_ = nullptr;
  int highlight_index_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_SHELL_STATUS_BAR_H_
