// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/debug/debug_console_panel.h"

#include <sstream>

#include "base/log/log_sink.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {

DebugConsolePanel::DebugConsolePanel() {
  set_preferred_size({0, 0});
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  box->set_inside_border(4, 4, 4, 4);
  box->set_between_child_spacing(4);

  auto output = std::make_unique<Label>("");
  output->set_preferred_size({0, 120});
  output_ = output.get();
  box->set_flex_for_view(output.get(), 1);

  auto input = std::make_unique<Textfield>();
  input->set_preferred_size({0, 24});
  input_ = input.get();
  input_->set_submit([this] { on_submit(); });

  set_layout_manager(std::move(box));
  add_child(std::move(output));
  add_child(std::move(input));
  apply_pane_mode();
}

DebugConsolePanel::~DebugConsolePanel() {
  drop_log_subscription();
}

void DebugConsolePanel::set_pane_mode(PaneMode mode) {
  if (mode_ == mode) {
    return;
  }
  mode_ = mode;
  apply_pane_mode();
}

void DebugConsolePanel::apply_pane_mode() {
  if (!input_) {
    return;
  }
  const bool show_input =
      mode_ == PaneMode::kCombined || mode_ == PaneMode::kConsole;
  input_->set_visible(show_input);
  input_->set_preferred_size(show_input ? Size{0, 24} : Size{0, 0});

  if (mode_ == PaneMode::kOutput || mode_ == PaneMode::kCombined) {
    if (visible_) {
      ensure_log_subscription();
    }
  } else {
    drop_log_subscription();
  }
  layout();
  schedule_paint();
}

void DebugConsolePanel::set_visible_console(bool on) {
  if (visible_ == on) {
    return;
  }
  visible_ = on;
  if (visible_) {
    set_preferred_size({0, 160});
    if (mode_ == PaneMode::kOutput || mode_ == PaneMode::kCombined) {
      ensure_log_subscription();
      const auto tail = base::log_sink().snapshot_tail(200);
      lines_.clear();
      for (const auto& e : tail) {
        lines_.push_back(std::string(base::log_level_name(e.level)) + " " +
                         e.message);
      }
      refresh_output_label();
    }
  } else {
    set_preferred_size({0, 0});
    drop_log_subscription();
  }
  if (View* p = parent()) {
    p->layout();
  } else {
    layout();
  }
  schedule_paint();
}

void DebugConsolePanel::append_line(std::string line) {
  lines_.push_back(std::move(line));
  if (lines_.size() > 500) {
    lines_.erase(lines_.begin(),
                 lines_.begin() + static_cast<std::ptrdiff_t>(lines_.size() - 500));
  }
  refresh_output_label();
}

void DebugConsolePanel::clear_output() {
  lines_.clear();
  refresh_output_label();
}

void DebugConsolePanel::set_submit_handler(
    std::function<void(const std::string&)> fn) {
  submit_ = std::move(fn);
}

void DebugConsolePanel::set_echo_to_output(
    std::function<void(const std::string&)> fn) {
  echo_to_output_ = std::move(fn);
}

void DebugConsolePanel::on_device_scale_factor_changed(float old_scale,
                                                      float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
}

void DebugConsolePanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas || !visible_) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
}

void DebugConsolePanel::on_submit() {
  if (!input_ || mode_ == PaneMode::kOutput) {
    return;
  }
  const std::string line = input_->text();
  input_->set_text("");
  append_line("> " + line);
  if (echo_to_output_) {
    echo_to_output_("> " + line);
  }
  if (submit_) {
    submit_(line);
  }
}

void DebugConsolePanel::refresh_output_label() {
  if (!output_) {
    return;
  }
  std::ostringstream oss;
  const size_t start = lines_.size() > 40 ? lines_.size() - 40 : 0;
  for (size_t i = start; i < lines_.size(); ++i) {
    if (i > start) {
      oss << '\n';
    }
    oss << lines_[i];
  }
  output_->set_text(oss.str());
  schedule_paint();
}

void DebugConsolePanel::ensure_log_subscription() {
  if (log_sub_id_ || mode_ == PaneMode::kConsole) {
    return;
  }
  log_sub_id_ = base::log_sink().subscribe([this](const base::LogEntry& e) {
    append_line(std::string(base::log_level_name(e.level)) + " " + e.message);
  });
}

void DebugConsolePanel::drop_log_subscription() {
  if (!log_sub_id_) {
    return;
  }
  base::log_sink().unsubscribe(log_sub_id_);
  log_sub_id_ = 0;
}

}  // namespace views
}  // namespace ui
