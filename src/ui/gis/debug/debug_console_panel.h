// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_DEBUG_DEBUG_CONSOLE_PANEL_H_
#define UI_GIS_DEBUG_DEBUG_CONSOLE_PANEL_H_

#include "ui/ui_export.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "base/log/log_sink.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class Checkbox;
class Label;
class ScrollView;
class Textfield;

// Debug Output / Console panes (QGIS Log Messages / Chromium DevTools feel).
// Used alone or inside Diagnostic Tools.
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
  bool on_key_event(const KeyEvent& event) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  // One retained log row (structured for level filter / colored paint).
  struct LogLine {
    base::LogLevel level = base::LogLevel::kInfo;
    std::string timestamp;
    std::string message;
  };

  class LogListView;

  friend class LogListView;

  void on_submit();
  void on_clear_clicked();
  void on_copy_clicked();
  void on_ask_clicked();
  void on_filter_changed();
  void ensure_log_subscription();
  void drop_log_subscription();
  void apply_pane_mode();
  void apply_frame_metrics(float scale);
  void append_entry(LogLine line);
  void enqueue_log_line(LogLine line);
  void flush_pending_logs();
  void arm_log_flush_timer();
  void disarm_log_flush_timer();
  static void CALLBACK on_log_flush_timer(HWND hwnd, UINT msg, UINT_PTR id,
                                          DWORD time);
  void reload_from_sink();
  void rebuild_visible_indices();
  void sync_list_size(bool stick_to_bottom);
  void scroll_to_bottom();
  bool line_passes_filters(const LogLine& line) const;
  float scale_factor() const;
  int row_height() const;
  std::string format_line_plain(const LogLine& line) const;
  bool on_input_key(const KeyEvent& event);
  void history_push(const std::string& line);
  bool history_navigate(int delta);
  bool try_tab_complete();

  PaneMode mode_ = PaneMode::kCombined;
  bool visible_ = false;
  bool auto_scroll_ = true;

  View* toolbar_ = nullptr;
  Button* clear_btn_ = nullptr;
  Button* copy_btn_ = nullptr;
  Button* ask_btn_ = nullptr;
  Checkbox* auto_scroll_cb_ = nullptr;
  Checkbox* show_error_ = nullptr;
  Checkbox* show_warn_ = nullptr;
  Checkbox* show_info_ = nullptr;
  Checkbox* show_debug_ = nullptr;
  Textfield* filter_ = nullptr;
  Label* count_ = nullptr;
  ScrollView* scroll_ = nullptr;
  LogListView* list_ = nullptr;
  Textfield* input_ = nullptr;

  std::vector<LogLine> lines_;
  std::vector<size_t> visible_indices_;
  int selected_visible_ = -1;
  std::string filter_text_;

  std::vector<std::string> history_;
  int history_index_ = -1;  // -1 = editing live buffer
  std::string history_draft_;

  std::function<void(const std::string&)> submit_;
  std::function<void(const std::string&)> echo_to_output_;
  std::uint64_t log_sub_id_ = 0;

  // LogSink notifies from any thread (FlyCube attach / present). Queue here
  // and flush on the Widget UI thread so lines_/visible_indices_ stay single
  // threaded (heap 0xC0000374 on Scene3D tab switch).
  std::mutex log_mu_;
  std::vector<LogLine> pending_logs_;
  std::atomic<bool> log_flush_armed_{false};
  bool shutting_down_ = false;
  bool flushing_logs_ = false;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_DEBUG_DEBUG_CONSOLE_PANEL_H_
