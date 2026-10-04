// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_SHELL_ATMOSPHERE_PANEL_H_
#define UI_GIS_SHELL_ATMOSPHERE_PANEL_H_

#include "ui/ui_export.h"
#include <functional>
#include <string>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Checkbox;
class Label;
class Slider;

// Minimal atmosphere shell: time scrub + ocean / cloud / sky / fog / wind.
// Hosts bridge callbacks to Scene3dPresenter / Environment; this panel never
// includes vista::atmosphere headers (toolkit stays map-agnostic).
class UI_EXPORT AtmospherePanel : public View {
 public:
  AtmospherePanel();
  ~AtmospherePanel() override;

  void set_time_range(double min_sec, double max_sec);
  void set_time_sec(double t);
  double time_sec() const;

  void set_ocean_checked(bool on);
  void set_cloud_checked(bool on);
  void set_sky_checked(bool on);
  void set_fog_checked(bool on);
  void set_wind_checked(bool on);
  bool ocean_checked() const;
  bool cloud_checked() const;
  bool sky_checked() const;
  bool fog_checked() const;
  bool wind_checked() const;

  void set_time_change(std::function<void(double)> fn);
  void set_ocean_change(std::function<void(bool)> fn);
  void set_cloud_change(std::function<void(bool)> fn);
  void set_sky_change(std::function<void(bool)> fn);
  void set_fog_change(std::function<void(bool)> fn);
  void set_wind_change(std::function<void(bool)> fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void refresh_time_label();

  Label* title_ = nullptr;
  Label* time_label_ = nullptr;
  Slider* scrub_ = nullptr;
  Checkbox* ocean_ = nullptr;
  Checkbox* cloud_ = nullptr;
  Checkbox* sky_ = nullptr;
  Checkbox* fog_ = nullptr;
  Checkbox* wind_ = nullptr;

  std::function<void(double)> time_change_;
  std::function<void(bool)> ocean_change_;
  std::function<void(bool)> cloud_change_;
  std::function<void(bool)> sky_change_;
  std::function<void(bool)> fog_change_;
  std::function<void(bool)> wind_change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_SHELL_ATMOSPHERE_PANEL_H_
