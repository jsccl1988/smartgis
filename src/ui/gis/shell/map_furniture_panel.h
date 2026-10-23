// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_SHELL_MAP_FURNITURE_PANEL_H_
#define UI_GIS_SHELL_MAP_FURNITURE_PANEL_H_

#include "ui/ui_export.h"

#include <functional>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Checkbox;
class Label;

// Map2d chrome toggles: scale bar, north arrow, neatline border.
// Eagle eye is a disabled placeholder. This panel never includes map2d
// or vista headers; the host wires callbacks through Map2dSink.
class UI_EXPORT MapFurniturePanel : public View {
 public:
  MapFurniturePanel();
  ~MapFurniturePanel() override;

  void set_scale_bar_checked(bool on);
  void set_north_arrow_checked(bool on);
  void set_border_checked(bool on);
  bool scale_bar_checked() const;
  bool north_arrow_checked() const;
  bool border_checked() const;
  bool eagle_eye_enabled() const;

  void set_scale_bar_change(std::function<void(bool)> fn);
  void set_north_arrow_change(std::function<void(bool)> fn);
  void set_border_change(std::function<void(bool)> fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  Label* title_ = nullptr;
  Checkbox* scale_bar_ = nullptr;
  Checkbox* north_arrow_ = nullptr;
  Checkbox* border_ = nullptr;
  Checkbox* eagle_eye_ = nullptr;

  std::function<void(bool)> scale_bar_change_;
  std::function<void(bool)> north_arrow_change_;
  std::function<void(bool)> border_change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_SHELL_MAP_FURNITURE_PANEL_H_
