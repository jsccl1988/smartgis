// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_DEBUG_DEBUG_CONSOLE_PANEL_H_
#define UI_GIS_DEBUG_DEBUG_CONSOLE_PANEL_H_

#include "ui/ui_export.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Label;
class Textfield;

// Debug Output / Console panes (VS-style). Used alone or inside Diagnostic Tools.
class UI_EXPORT DebugConsolePanel : public View {
 public:
  enum class PaneMode {
    kCombined,  // log + input (legacy single panel)
    kOutput,    // LogSink only
    kConsole,   // input + echo only
  };

  DebugConsolePanel();
  ~DebugConsolePanel() override;

  void set_pane_mode(PaneMode mode);
  PaneMode pane_mode() const { return mode_; }

  void set_visible_console(bool on);
  bool is_console_visible() const { return visible_; }

  void append_line(std::string line);
  void clear_output();

  // Called with the submitted input line (before clear).
  void set_submit_handler(std::function<void(const std::string&)> fn);

  void set_echo_to_output(std::function<void(const std::string&)> fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void on_submit();
  void refresh_output_label();
  void ensure_log_subscription();
  void drop_log_subscription();
  void apply_pane_mode();

  PaneMode mode_ = PaneMode::kCombined;
  bool visible_ = false;
  Label* output_ = nullptr;
  Textfield* input_ = nullptr;
  std::vector<std::string> lines_;
  std::function<void(const std::string&)> submit_;
  std::function<void(const std::string&)> echo_to_output_;
  std::uint64_t log_sub_id_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_DEBUG_DEBUG_CONSOLE_PANEL_H_
