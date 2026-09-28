// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/gis/panel/atmosphere_panel.h"

#include <format>
#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/input/slider.h"

namespace ui {
namespace views {

AtmospherePanel::AtmospherePanel() {
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  box->set_inside_border(8, 6, 8, 6);
  box->set_between_child_spacing(4);

  auto title = std::make_unique<Label>("Atmosphere");
  title->set_preferred_size({200, 22});
  title_ = title.get();

  auto time_label = std::make_unique<Label>("t = 0.0 s");
  time_label->set_preferred_size({200, 20});
  time_label_ = time_label.get();

  auto scrub = std::make_unique<Slider>();
  scrub->set_preferred_size({240, 24});
  scrub->set_range(0.0, 3600.0);
  scrub->set_value(0.0);
  scrub_ = scrub.get();
  scrub_->set_change([this](double t) {
    refresh_time_label();
    if (time_change_) {
      time_change_(t);
    }
  });

  auto ocean = std::make_unique<Checkbox>("Ocean");
  ocean->set_preferred_size({200, 22});
  ocean_ = ocean.get();
  ocean_->set_change([this](bool on) {
    if (ocean_change_) {
      ocean_change_(on);
    }
  });

  auto cloud = std::make_unique<Checkbox>("Cloud");
  cloud->set_preferred_size({200, 22});
  cloud_ = cloud.get();
  cloud_->set_change([this](bool on) {
    if (cloud_change_) {
      cloud_change_(on);
    }
  });

  auto sky = std::make_unique<Checkbox>("Sky");
  sky->set_preferred_size({200, 22});
  sky_ = sky.get();
  sky_->set_change([this](bool on) {
    if (sky_change_) {
      sky_change_(on);
    }
  });

  auto fog = std::make_unique<Checkbox>("Fog");
  fog->set_preferred_size({200, 22});
  fog_ = fog.get();
  fog_->set_change([this](bool on) {
    if (fog_change_) {
      fog_change_(on);
    }
  });

  auto wind = std::make_unique<Checkbox>("Wind arrows");
  wind->set_preferred_size({200, 22});
  wind_ = wind.get();
  wind_->set_change([this](bool on) {
    if (wind_change_) {
      wind_change_(on);
    }
  });

  set_layout_manager(std::move(box));
  add_child(std::move(title));
  add_child(std::move(time_label));
  add_child(std::move(scrub));
  add_child(std::move(ocean));
  add_child(std::move(cloud));
  add_child(std::move(sky));
  add_child(std::move(fog));
  add_child(std::move(wind));
  set_preferred_size({260, 210});
  refresh_time_label();
}

AtmospherePanel::~AtmospherePanel() {
  // Children hold lambdas that capture `this` and call member std::functions.
  // Drop them before those members are destroyed (member order is reverse of
  // declaration; ~View clears children_ only after derived members).
  if (scrub_) {
    scrub_->set_change({});
  }
  if (ocean_) {
    ocean_->set_change({});
  }
  if (cloud_) {
    cloud_->set_change({});
  }
  if (sky_) {
    sky_->set_change({});
  }
  if (fog_) {
    fog_->set_change({});
  }
  if (wind_) {
    wind_->set_change({});
  }
  remove_all_children();
  scrub_ = nullptr;
  time_label_ = nullptr;
  title_ = nullptr;
  ocean_ = nullptr;
  cloud_ = nullptr;
  sky_ = nullptr;
  fog_ = nullptr;
  wind_ = nullptr;
}

void AtmospherePanel::set_time_range(double min_sec, double max_sec) {
  if (scrub_) {
    scrub_->set_range(min_sec, max_sec);
  }
  refresh_time_label();
}

void AtmospherePanel::set_time_sec(double t) {
  if (scrub_) {
    scrub_->set_value(t);
  }
  refresh_time_label();
}

double AtmospherePanel::time_sec() const {
  return scrub_ ? scrub_->value() : 0.0;
}

void AtmospherePanel::set_ocean_checked(bool on) {
  if (ocean_) {
    ocean_->set_checked(on);
  }
}

void AtmospherePanel::set_cloud_checked(bool on) {
  if (cloud_) {
    cloud_->set_checked(on);
  }
}

void AtmospherePanel::set_sky_checked(bool on) {
  if (sky_) {
    sky_->set_checked(on);
  }
}

void AtmospherePanel::set_fog_checked(bool on) {
  if (fog_) {
    fog_->set_checked(on);
  }
}

void AtmospherePanel::set_wind_checked(bool on) {
  if (wind_) {
    wind_->set_checked(on);
  }
}

bool AtmospherePanel::ocean_checked() const {
  return ocean_ && ocean_->is_checked();
}

bool AtmospherePanel::cloud_checked() const {
  return cloud_ && cloud_->is_checked();
}

bool AtmospherePanel::sky_checked() const {
  return sky_ && sky_->is_checked();
}

bool AtmospherePanel::fog_checked() const {
  return fog_ && fog_->is_checked();
}

bool AtmospherePanel::wind_checked() const {
  return wind_ && wind_->is_checked();
}

void AtmospherePanel::set_time_change(std::function<void(double)> fn) {
  time_change_ = std::move(fn);
}

void AtmospherePanel::set_ocean_change(std::function<void(bool)> fn) {
  ocean_change_ = std::move(fn);
}

void AtmospherePanel::set_cloud_change(std::function<void(bool)> fn) {
  cloud_change_ = std::move(fn);
}

void AtmospherePanel::set_sky_change(std::function<void(bool)> fn) {
  sky_change_ = std::move(fn);
}

void AtmospherePanel::set_fog_change(std::function<void(bool)> fn) {
  fog_change_ = std::move(fn);
}

void AtmospherePanel::set_wind_change(std::function<void(bool)> fn) {
  wind_change_ = std::move(fn);
}

void AtmospherePanel::refresh_time_label() {
  if (!time_label_ || !scrub_) {
    return;
  }
  time_label_->set_text(std::format("t = {:.1f} s", scrub_->value()));
}

void AtmospherePanel::on_device_scale_factor_changed(float old_scale,
                                                    float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(260, s), dip_to_px(210, s)});
}

void AtmospherePanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
