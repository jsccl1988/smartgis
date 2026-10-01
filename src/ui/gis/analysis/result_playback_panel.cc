// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/analysis/result_playback_panel.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/input/slider.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {

ResultPlaybackPanel::ResultPlaybackPanel() {
  MarkupRoot loaded = load_markup("analysis/result_playback_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({280, 160});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  frame_label_ = loaded.ids.find_as<Label>("frame");
  status_ = loaded.ids.find_as<Label>("status");
  play_ = loaded.ids.find_as<Button>("play");
  prev_ = loaded.ids.find_as<Button>("prev");
  next_ = loaded.ids.find_as<Button>("next");
  loop_ = loaded.ids.find_as<Checkbox>("loop");
  scrub_ = loaded.ids.find_as<Slider>("scrub");

  if (play_) {
    play_->set_click([this]() { on_play_clicked(); });
  }
  if (prev_) {
    prev_->set_click([this]() { on_prev_clicked(); });
  }
  if (next_) {
    next_->set_click([this]() { on_next_clicked(); });
  }
  if (loop_) {
    loop_->set_checked(true);
    loop_->set_change([this](bool on) {
      if (loop_change_) {
        loop_change_(on);
      }
    });
  }
  if (scrub_) {
    scrub_->set_range(0.0, 0.0);
    scrub_->set_value(0.0);
    scrub_->set_change([this](double v) { on_scrub(v); });
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({280, 160});
  add_child(std::move(loaded.root));
  set_preferred_size({280, 160});
  refresh_labels();
}

ResultPlaybackPanel::~ResultPlaybackPanel() {
  if (play_) {
    play_->set_click({});
  }
  if (prev_) {
    prev_->set_click({});
  }
  if (next_) {
    next_->set_click({});
  }
  if (loop_) {
    loop_->set_change({});
  }
  if (scrub_) {
    scrub_->set_change({});
  }
  remove_all_children();
  title_ = nullptr;
  frame_label_ = nullptr;
  status_ = nullptr;
  play_ = nullptr;
  prev_ = nullptr;
  next_ = nullptr;
  loop_ = nullptr;
  scrub_ = nullptr;
}

void ResultPlaybackPanel::set_frame_range(int frame_count) {
  frame_count_ = std::max(0, frame_count);
  if (frame_count_ <= 0) {
    frame_index_ = 0;
  } else {
    frame_index_ = std::clamp(frame_index_, 0, frame_count_ - 1);
  }
  if (scrub_) {
    scrub_notify_ = false;
    if (frame_count_ <= 1) {
      scrub_->set_range(0.0, 0.0);
      scrub_->set_value(0.0);
    } else {
      scrub_->set_range(0.0, static_cast<double>(frame_count_ - 1));
      scrub_->set_value(static_cast<double>(frame_index_));
    }
    scrub_notify_ = true;
  }
  refresh_labels();
}

void ResultPlaybackPanel::set_frame_index(int index) {
  if (frame_count_ <= 0) {
    frame_index_ = 0;
  } else {
    frame_index_ = std::clamp(index, 0, frame_count_ - 1);
  }
  if (scrub_) {
    scrub_notify_ = false;
    scrub_->set_value(static_cast<double>(frame_index_));
    scrub_notify_ = true;
  }
  refresh_labels();
}

void ResultPlaybackPanel::set_playing(bool on) {
  playing_ = on;
  if (play_) {
    play_->set_text(playing_ ? "Pause" : "Play");
  }
}

bool ResultPlaybackPanel::looping() const {
  return loop_ ? loop_->is_checked() : true;
}

void ResultPlaybackPanel::set_looping(bool on) {
  if (loop_) {
    loop_->set_checked(on);
  }
}

void ResultPlaybackPanel::set_status_text(std::string text) {
  if (status_) {
    status_->set_text(std::move(text));
  }
}

void ResultPlaybackPanel::set_frame_change(std::function<void(int)> fn) {
  frame_change_ = std::move(fn);
}

void ResultPlaybackPanel::set_play_change(std::function<void(bool)> fn) {
  play_change_ = std::move(fn);
}

void ResultPlaybackPanel::set_loop_change(std::function<void(bool)> fn) {
  loop_change_ = std::move(fn);
}

void ResultPlaybackPanel::refresh_labels() {
  if (frame_label_) {
    frame_label_->set_text(
        std::format("frame {} / {}", frame_index_, frame_count_));
  }
}

void ResultPlaybackPanel::on_play_clicked() {
  set_playing(!playing_);
  if (play_change_) {
    play_change_(playing_);
  }
}

void ResultPlaybackPanel::on_prev_clicked() {
  if (frame_count_ <= 0) {
    return;
  }
  int next = frame_index_ - 1;
  if (next < 0) {
    next = looping() ? frame_count_ - 1 : 0;
  }
  set_frame_index(next);
  if (frame_change_) {
    frame_change_(frame_index_);
  }
}

void ResultPlaybackPanel::on_next_clicked() {
  if (frame_count_ <= 0) {
    return;
  }
  int next = frame_index_ + 1;
  if (next >= frame_count_) {
    next = looping() ? 0 : frame_count_ - 1;
  }
  set_frame_index(next);
  if (frame_change_) {
    frame_change_(frame_index_);
  }
}

void ResultPlaybackPanel::on_scrub(double value) {
  if (!scrub_notify_ || frame_count_ <= 0) {
    return;
  }
  const int index = static_cast<int>(std::lround(value));
  if (index == frame_index_) {
    return;
  }
  set_frame_index(index);
  if (frame_change_) {
    frame_change_(frame_index_);
  }
}

void ResultPlaybackPanel::on_device_scale_factor_changed(float old_scale,
                                                       float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(280, s), dip_to_px(160, s)});
}

void ResultPlaybackPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
