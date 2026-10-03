// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_DEBUG_DIAGNOSTIC_TOOLS_PANEL_H_
#define UI_GIS_DEBUG_DIAGNOSTIC_TOOLS_PANEL_H_

#include "ui/ui_export.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class Checkbox;
class DebugConsolePanel;
class Label;
class RenderTracePanel;
class TabStrip;

// VS-style bottom Diagnostic Tools: Output | Console | Trace | Memory.
// Trace = RenderTracePanel (UI Views paint/compositor + map/GDI filters).
// Allocate via make_diagnostic_tools_panel() for cross-module View ownership.
class UI_EXPORT DiagnosticToolsPanel : public View {
 public:
  DiagnosticToolsPanel();
  ~DiagnosticToolsPanel() override;

  void set_visible_tools(bool on);
  bool is_tools_visible() const { return visible_; }

  // Select Output / Console / Trace / Memory (no-op if index invalid).
  void set_active_tab(int index);
  int active_tab() const;

  DebugConsolePanel* output_pane() { return output_; }
  DebugConsolePanel* console_pane() { return console_; }
  RenderTracePanel* cpu_pane() { return cpu_; }

  void set_console_submit(std::function<void(const std::string&)> fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void apply_frame_metrics(float scale);
  void reseed_host_splitter();
  void on_record();
  void on_stop();
  void on_clear();
  void on_export();
  void on_refresh();
  void sample_memory();
  void refresh_memory_stats();
  void update_status();
  void maybe_auto_refresh();

  bool visible_ = false;
  View* panel_root_ = nullptr;
  View* toolbar_ = nullptr;
  View* tabs_host_ = nullptr;
  BoxLayout* panel_box_ = nullptr;
  Label* title_ = nullptr;
  Label* status_ = nullptr;
  Label* memory_stats_ = nullptr;
  Button* record_ = nullptr;
  Button* stop_ = nullptr;
  Button* clear_ = nullptr;
  Button* export_ = nullptr;
  Button* refresh_ = nullptr;
  Checkbox* arm_ = nullptr;
  Checkbox* track_allocs_ = nullptr;
  Checkbox* echo_commands_ = nullptr;
  TabStrip* tabs_ = nullptr;
  DebugConsolePanel* output_ = nullptr;
  DebugConsolePanel* console_ = nullptr;
  RenderTracePanel* cpu_ = nullptr;
  View* memory_page_ = nullptr;
  std::chrono::steady_clock::time_point last_auto_refresh_{};
};

UI_EXPORT std::unique_ptr<DiagnosticToolsPanel>
make_diagnostic_tools_panel();

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_DEBUG_DIAGNOSTIC_TOOLS_PANEL_H_
