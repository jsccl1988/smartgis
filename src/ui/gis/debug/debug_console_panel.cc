// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/debug/debug_console_panel.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <memory>
#include <utility>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/event.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace {

constexpr int kRowHeightDip = 20;
constexpr int kToolbarHeightDip = 24;
constexpr int kInputHeightDip = 24;
constexpr int kMaxLines = 4000;
constexpr int kMaxHistory = 200;
constexpr int kConsolePreferredDip = 200;
constexpr int kLogHostMinDip = 72;
constexpr int kPadDip = 4;
constexpr int kLevelColDip = 52;
constexpr int kTimeColDip = 72;
constexpr int kHairlineDip = 1;

// Console :command prefixes for Tab completion (keep aligned with Agent).
constexpr const char* kCmdPrefixes[] = {
    ":help",
    ":help json",
    ":clear",
    ":confirm",
    ":log.level ",
    ":refresh",
    ":extent",
    ":layers",
    ":diag",
    ":diag capture",
    ":ask ",
    ":ui find ",
    ":ui click ",
    ":ui type ",
    ":ui tree",
    ":ui overlay",
    ":ui capture",
    ":script ",
    ":record on",
    ":record off",
    ":record poll",
    ":record clear",
    ":gis test",
    ":gis bench",
    ":rhi test",
    ":rhi bench",
    ":sdbd capabilities",
    ":sdbd sql ",
    ":py ",
    ":run ",
};

ui::gfx::Color level_color(base::LogLevel level, const Theme& t) {
  switch (level) {
    case base::LogLevel::kFatal:
    case base::LogLevel::kError:
      return 0xfff56c6cu;
    case base::LogLevel::kWarning:
      return 0xffe6a23cu;
    case base::LogLevel::kNotice:
      return 0xff4c8bf5u;
    case base::LogLevel::kInfo:
      return t.text;
    case base::LogLevel::kDebug:
    case base::LogLevel::kTrace:
      return t.text_muted;
  }
  return t.text;
}

const char* level_tag(base::LogLevel level) {
  switch (level) {
    case base::LogLevel::kFatal:
      return "FATAL";
    case base::LogLevel::kError:
      return "ERROR";
    case base::LogLevel::kWarning:
      return "WARN";
    case base::LogLevel::kNotice:
      return "NOTE";
    case base::LogLevel::kInfo:
      return "INFO";
    case base::LogLevel::kDebug:
      return "DEBUG";
    case base::LogLevel::kTrace:
      return "TRACE";
  }
  return "INFO";
}

bool contains_ci(const std::string& hay, const std::string& needle) {
  if (needle.empty()) {
    return true;
  }
  auto lower = [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  };
  if (needle.size() > hay.size()) {
    return false;
  }
  for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
    bool ok = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      if (lower(static_cast<unsigned char>(hay[i + j])) !=
          lower(static_cast<unsigned char>(needle[j]))) {
        ok = false;
        break;
      }
    }
    if (ok) {
      return true;
    }
  }
  return false;
}

bool copy_utf8_to_clipboard(const std::string& utf8) {
  if (utf8.empty()) {
    return false;
  }
  const int chars = MultiByteToWideChar(
      CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (chars <= 0) {
    return false;
  }
  std::wstring wide(static_cast<size_t>(chars), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                      wide.data(), chars);
  if (!OpenClipboard(nullptr)) {
    return false;
  }
  EmptyClipboard();
  const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!mem) {
    CloseClipboard();
    return false;
  }
  void* locked = GlobalLock(mem);
  if (!locked) {
    GlobalFree(mem);
    CloseClipboard();
    return false;
  }
  std::memcpy(locked, wide.c_str(), bytes);
  GlobalUnlock(mem);
  const bool ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
  if (!ok) {
    GlobalFree(mem);
  }
  CloseClipboard();
  return ok;
}

}  // namespace

// Virtualized-feel log list: paints only rows intersecting the exposed clip.
class DebugConsolePanel::LogListView : public View {
 public:
  explicit LogListView(DebugConsolePanel* owner) : owner_(owner) {
    set_focusable(true);
    set_preferred_size({0, 0});
  }

  bool on_mouse_event(const MouseEvent& event) override {
    if (!owner_ || !is_enabled()) {
      return false;
    }
    if (event.type == MouseEvent::Type::kDown && event.button == 1) {
      request_focus();
      const int rh = owner_->row_height();
      if (rh > 0) {
        const int local_y = event.y - bounds().y;
        const int idx = local_y / rh;
        if (idx >= 0 &&
            idx < static_cast<int>(owner_->visible_indices_.size())) {
          owner_->selected_visible_ = idx;
          schedule_paint();
        }
      }
      return true;
    }
    return false;
  }

  bool on_key_event(const KeyEvent& event) override {
    if (!owner_ || event.type != KeyEvent::Type::kDown) {
      return false;
    }
    const int rh = owner_->row_height();
    if (!owner_->scroll_ || rh <= 0) {
      return false;
    }
    const int page = std::max(rh, bounds().height);
    int next = owner_->scroll_->scroll_offset();
    switch (event.vk) {
      case VK_UP:
        next -= rh;
        break;
      case VK_DOWN:
        next += rh;
        break;
      case VK_PRIOR:
        next -= page;
        break;
      case VK_NEXT:
        next += page;
        break;
      case VK_HOME:
        next = 0;
        break;
      case VK_END:
        next = preferred_size().height;
        break;
      default:
        return false;
    }
    owner_->auto_scroll_ = false;
    if (owner_->auto_scroll_cb_) {
      owner_->auto_scroll_cb_->set_checked(false);
    }
    owner_->scroll_->set_scroll_offset(next);
    return true;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas || !owner_) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    const float scale = owner_->scale_factor();
    const int rh = owner_->row_height();
    const int pad = dip_to_px(kPadDip, scale);
    const int time_w = dip_to_px(kTimeColDip, scale);
    const int level_w = dip_to_px(kLevelColDip, scale);
    const int hair = dip_to_px(kHairlineDip, scale);

    canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);

    // Prefer exposed_rect from ScrollView when present so off-screen rows skip.
    Rect clip = b;
    if (exposed_rect().width > 0 && exposed_rect().height > 0) {
      clip = exposed_rect();
    }
    const int first = std::max(0, (clip.y - b.y) / std::max(1, rh));
    const int last =
        std::min(static_cast<int>(owner_->visible_indices_.size()),
                 (clip.bottom() - b.y + rh - 1) / std::max(1, rh) + 1);

    for (int vi = first; vi < last; ++vi) {
      const size_t li = owner_->visible_indices_[static_cast<size_t>(vi)];
      if (li >= owner_->lines_.size()) {
        continue;
      }
      const LogLine& line = owner_->lines_[li];
      const int y = b.y + vi * rh;
      const Rect row{b.x, y, b.width, rh};

      if (vi == owner_->selected_visible_) {
        canvas->fill_rect(row.x, row.y, row.width, row.height, t.control_hover);
        canvas->fill_rect(row.x, row.y, dip_to_px(3, scale), row.height,
                          t.accent);
      } else if ((vi % 2) == 1) {
        // Subtle zebra — ArcGIS/QGIS density without loud stripes.
        canvas->fill_rect(row.x, row.y, row.width, row.height, t.panel_bg);
      }

      if (hair > 0) {
        canvas->fill_rect(row.x + pad, row.bottom() - hair,
                          std::max(0, row.width - 2 * pad), hair,
                          t.panel_header);
      }

      const int text_y =
          row.y + std::max(0, (rh - shell_body_font_px(scale)) / 2);
      int x = row.x + pad;

      if (!line.timestamp.empty()) {
        canvas->save();
        canvas->clip_rect(x, row.y, time_w, rh);
        canvas->draw_text(x, text_y, utf8_to_wide(line.timestamp).c_str(),
                          t.text_muted);
        canvas->restore();
      }
      x += time_w;

      const ui::gfx::Color lc = level_color(line.level, t);
      canvas->save();
      canvas->clip_rect(x, row.y, level_w, rh);
      canvas->draw_text(x, text_y, utf8_to_wide(level_tag(line.level)).c_str(),
                        lc);
      canvas->restore();
      x += level_w + pad;

      const int msg_w = std::max(0, row.right() - pad - x);
      if (msg_w > 0) {
        canvas->save();
        canvas->clip_rect(x, row.y, msg_w, rh);
        canvas->draw_text(x, text_y, utf8_to_wide(line.message).c_str(),
                          line.level == base::LogLevel::kError ||
                                  line.level == base::LogLevel::kFatal
                              ? lc
                              : t.text);
        canvas->restore();
      }
    }

    if (is_focused()) {
      const int inset = std::max(1, dip_to_px(1, scale));
      draw_focus_ring(canvas, {b.x + inset, b.y + inset,
                               std::max(0, b.width - 2 * inset),
                               std::max(0, b.height - 2 * inset)});
    }
  }

 private:
  DebugConsolePanel* owner_ = nullptr;
};

DebugConsolePanel::DebugConsolePanel() {
  MarkupRoot loaded = load_markup("debug/debug_console_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({0, 0});
    return;
  }

  toolbar_ = loaded.ids.find("toolbar");
  clear_btn_ = loaded.ids.find_as<Button>("clear");
  copy_btn_ = loaded.ids.find_as<Button>("copy");
  ask_btn_ = loaded.ids.find_as<Button>("ask");
  auto_scroll_cb_ = loaded.ids.find_as<Checkbox>("auto_scroll");
  show_error_ = loaded.ids.find_as<Checkbox>("show_error");
  show_warn_ = loaded.ids.find_as<Checkbox>("show_warn");
  show_info_ = loaded.ids.find_as<Checkbox>("show_info");
  show_debug_ = loaded.ids.find_as<Checkbox>("show_debug");
  filter_ = loaded.ids.find_as<Textfield>("filter");
  count_ = loaded.ids.find_as<Label>("count");
  View* log_host = loaded.ids.find("log_host");
  input_ = loaded.ids.find_as<Textfield>("input");

  if (clear_btn_) {
    clear_btn_->set_click([this] { on_clear_clicked(); });
  }
  if (copy_btn_) {
    copy_btn_->set_click([this] { on_copy_clicked(); });
  }
  if (ask_btn_) {
    ask_btn_->set_click([this] { on_ask_clicked(); });
  }
  if (auto_scroll_cb_) {
    auto_scroll_cb_->set_checked(true);
    auto_scroll_cb_->set_change([this](bool on) { auto_scroll_ = on; });
  }
  auto refilter = [this](bool) { on_filter_changed(); };
  if (show_error_) {
    show_error_->set_change(refilter);
  }
  if (show_warn_) {
    show_warn_->set_change(refilter);
  }
  if (show_info_) {
    show_info_->set_change(refilter);
  }
  if (show_debug_) {
    show_debug_->set_change(refilter);
  }
  if (filter_) {
    filter_->set_placeholder("Filter…");
    filter_->set_change([this] { on_filter_changed(); });
  }
  if (input_) {
    input_->set_placeholder(":help  ·  Up/Down history  ·  Tab complete");
    input_->set_submit([this] { on_submit(); });
    input_->set_key_hook([this](const KeyEvent& e) { return on_input_key(e); });
  }

  auto scroll = std::make_unique<ScrollView>();
  scroll_ = scroll.get();
  auto list = std::make_unique<LogListView>(this);
  list_ = list.get();
  scroll_->add_child(std::move(list));
  // Wheel / track leave the bottom → stop auto-stick so new lines do not yank.
  // Programmatic stick (offset at max) leaves auto_scroll_ alone.
  scroll_->set_on_scroll([this](int offset) {
    if (!scroll_ || !list_) {
      return;
    }
    const int max_y =
        std::max(0, list_->preferred_size().height - scroll_->bounds().height);
    if (offset >= max_y) {
      return;
    }
    auto_scroll_ = false;
    if (auto_scroll_cb_) {
      auto_scroll_cb_->set_checked(false);
    }
  });
  if (log_host) {
    log_host->set_layout_manager(std::make_unique<FillLayout>());
    log_host->add_child(std::move(scroll));
  }

  if (log_host) {
    log_host->set_preferred_size({0, kLogHostMinDip});
  }
  if (View* root = loaded.root.get()) {
    auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
    if (log_host) {
      box->set_flex_for_view(log_host, 1);
    }
    box->set_between_child_spacing(dip_to_px(2, 1.f));
    root->set_layout_manager(std::move(box));
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({0, kConsolePreferredDip});
  add_child(std::move(loaded.root));
  set_preferred_size({0, 0});
  set_focusable(true);
  apply_frame_metrics(1.f);
  apply_pane_mode();
  rebuild_visible_indices();
  sync_list_size(true);
}

DebugConsolePanel::~DebugConsolePanel() {
  {
    std::lock_guard<std::mutex> lock(log_mu_);
    shutting_down_ = true;
    pending_logs_.clear();
  }
  drop_log_subscription();
  disarm_log_flush_timer();
  if (clear_btn_) {
    clear_btn_->set_click({});
  }
  if (copy_btn_) {
    copy_btn_->set_click({});
  }
  if (ask_btn_) {
    ask_btn_->set_click({});
  }
  if (auto_scroll_cb_) {
    auto_scroll_cb_->set_change({});
  }
  if (show_error_) {
    show_error_->set_change({});
  }
  if (show_warn_) {
    show_warn_->set_change({});
  }
  if (show_info_) {
    show_info_->set_change({});
  }
  if (show_debug_) {
    show_debug_->set_change({});
  }
  if (filter_) {
    filter_->set_change({});
  }
  if (input_) {
    input_->set_submit({});
    input_->set_key_hook({});
  }
  remove_all_children();
  toolbar_ = nullptr;
  clear_btn_ = nullptr;
  copy_btn_ = nullptr;
  ask_btn_ = nullptr;
  auto_scroll_cb_ = nullptr;
  show_error_ = nullptr;
  show_warn_ = nullptr;
  show_info_ = nullptr;
  show_debug_ = nullptr;
  filter_ = nullptr;
  count_ = nullptr;
  scroll_ = nullptr;
  list_ = nullptr;
  input_ = nullptr;
}

void DebugConsolePanel::set_pane_mode(PaneMode mode) {
  if (mode_ == mode) {
    return;
  }
  mode_ = mode;
  apply_pane_mode();
}

void DebugConsolePanel::apply_pane_mode() {
  if (input_) {
    const bool show_input =
        mode_ == PaneMode::kCombined || mode_ == PaneMode::kConsole;
    input_->set_visible(show_input);
    apply_frame_metrics(scale_factor());
  }

  // Level filters only matter when LogSink is feeding the pane.
  const bool log_mode =
      mode_ == PaneMode::kOutput || mode_ == PaneMode::kCombined;
  if (show_error_) {
    show_error_->set_visible(log_mode);
  }
  if (show_warn_) {
    show_warn_->set_visible(log_mode);
  }
  if (show_info_) {
    show_info_->set_visible(log_mode);
  }
  if (show_debug_) {
    show_debug_->set_visible(log_mode);
  }

  if (log_mode) {
    if (visible_) {
      ensure_log_subscription();
    }
  } else {
    drop_log_subscription();
  }
  layout();
  schedule_paint();
}

void DebugConsolePanel::apply_frame_metrics(float scale) {
  if (scale <= 0.f) {
    scale = 1.f;
  }
  auto set_pref = [&](View* v, int w_dip, int h_dip) {
    if (!v) {
      return;
    }
    const int w = w_dip > 0 ? dip_to_px(w_dip, scale) : 0;
    v->set_preferred_size({w, dip_to_px(h_dip, scale)});
  };
  set_pref(toolbar_, 0, kToolbarHeightDip);
  set_pref(clear_btn_, 52, 22);
  set_pref(copy_btn_, 52, 22);
  set_pref(ask_btn_, 44, 22);
  // Checkbox preferred width comes from measured label (do not clip Auto-scroll).
  if (auto_scroll_cb_) {
    auto_scroll_cb_->set_label("Auto-scroll");
  }
  set_pref(show_error_, 28, 20);
  set_pref(show_warn_, 28, 20);
  set_pref(show_info_, 28, 20);
  set_pref(show_debug_, 28, 20);
  set_pref(filter_, 140, 20);
  set_pref(count_, 48, 20);
  if (View* log_host = scroll_ ? scroll_->parent() : nullptr) {
    set_pref(log_host, 0, kLogHostMinDip);
  }
  const bool show_input =
      input_ && (mode_ == PaneMode::kCombined || mode_ == PaneMode::kConsole) &&
      input_->is_locally_visible();
  set_pref(input_, 0, show_input ? kInputHeightDip : 0);
  if (toolbar_) {
    if (auto* box = dynamic_cast<BoxLayout*>(toolbar_->layout_manager())) {
      box->set_between_child_spacing(dip_to_px(6, scale));
    } else {
      auto row = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
      row->set_between_child_spacing(dip_to_px(6, scale));
      toolbar_->set_layout_manager(std::move(row));
    }
  }
}

void DebugConsolePanel::set_visible_console(bool on) {
  if (visible_ == on) {
    return;
  }
  visible_ = on;
  if (visible_) {
    set_preferred_size({0, kConsolePreferredDip});
    apply_frame_metrics(scale_factor());
    if (mode_ == PaneMode::kOutput || mode_ == PaneMode::kCombined) {
      ensure_log_subscription();
      reload_from_sink();
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
  LogLine entry;
  entry.level = base::LogLevel::kInfo;
  entry.message = std::move(line);
  enqueue_log_line(std::move(entry));
}

void DebugConsolePanel::append_entry(LogLine line) {
  lines_.push_back(std::move(line));
  if (lines_.size() > kMaxLines) {
    const auto drop =
        static_cast<std::ptrdiff_t>(lines_.size() - kMaxLines);
    lines_.erase(lines_.begin(), lines_.begin() + drop);
    selected_visible_ = -1;
  }
}

void DebugConsolePanel::enqueue_log_line(LogLine line) {
  {
    std::lock_guard<std::mutex> lock(log_mu_);
    if (shutting_down_) {
      return;
    }
    pending_logs_.push_back(std::move(line));
  }
  HWND hwnd = widget() ? widget()->hwnd() : nullptr;
  if (hwnd && GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId()) {
    arm_log_flush_timer();
    return;
  }
  flush_pending_logs();
}

void DebugConsolePanel::flush_pending_logs() {
  if (flushing_logs_) {
    return;
  }
  flushing_logs_ = true;
  bool any = false;
  for (;;) {
    std::vector<LogLine> batch;
    {
      std::lock_guard<std::mutex> lock(log_mu_);
      if (shutting_down_) {
        pending_logs_.clear();
        break;
      }
      batch.swap(pending_logs_);
    }
    if (batch.empty()) {
      break;
    }
    any = true;
    for (LogLine& line : batch) {
      append_entry(std::move(line));
    }
  }
  if (any) {
    rebuild_visible_indices();
    sync_list_size(auto_scroll_);
  }
  flushing_logs_ = false;
}

void DebugConsolePanel::arm_log_flush_timer() {
  HWND hwnd = widget() ? widget()->hwnd() : nullptr;
  if (!hwnd) {
    return;
  }
  if (log_flush_armed_.exchange(true)) {
    return;
  }
  if (!SetTimer(hwnd, reinterpret_cast<UINT_PTR>(this), USER_TIMER_MINIMUM,
                &DebugConsolePanel::on_log_flush_timer)) {
    log_flush_armed_.store(false);
  }
}

void DebugConsolePanel::disarm_log_flush_timer() {
  log_flush_armed_.store(false);
  HWND hwnd = widget() ? widget()->hwnd() : nullptr;
  if (hwnd) {
    KillTimer(hwnd, reinterpret_cast<UINT_PTR>(this));
  }
}

void CALLBACK DebugConsolePanel::on_log_flush_timer(HWND hwnd, UINT, UINT_PTR id,
                                                    DWORD) {
  KillTimer(hwnd, id);
  auto* self = reinterpret_cast<DebugConsolePanel*>(id);
  if (!self) {
    return;
  }
  self->log_flush_armed_.store(false);
  self->flush_pending_logs();
}

void DebugConsolePanel::clear_output() {
  lines_.clear();
  visible_indices_.clear();
  selected_visible_ = -1;
  rebuild_visible_indices();
  sync_list_size(true);
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
  for (size_t i = 0; i < child_count(); ++i) {
    if (View* c = child_at(i)) {
      c->propagate_device_scale_factor_changed(old_scale, new_scale);
    }
  }
  apply_frame_metrics(new_scale);
  sync_list_size(false);
}

bool DebugConsolePanel::on_key_event(const KeyEvent& event) {
  if (list_ && list_->on_key_event(event)) {
    return true;
  }
  return View::on_key_event(event);
}

void DebugConsolePanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas || !visible_) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  // Top hairline separates console from host horizon (catalog/layer density).
  const int hair = dip_to_px(kHairlineDip, scale_factor());
  if (hair > 0 && b.width > 0) {
    canvas->fill_rect(b.x, b.y, b.width, hair, t.panel_header);
  }
}

void DebugConsolePanel::on_submit() {
  if (!input_ || mode_ == PaneMode::kOutput) {
    return;
  }
  const std::string line = input_->text();
  input_->set_text("");
  history_push(line);
  append_line("> " + line);
  if (echo_to_output_) {
    echo_to_output_("> " + line);
  }
  if (submit_) {
    submit_(line);
  }
}

void DebugConsolePanel::on_ask_clicked() {
  if (!input_ || mode_ == PaneMode::kOutput) {
    return;
  }
  const std::string cur = input_->text();
  if (cur.empty() || cur == ":ask" || cur == ":ask ") {
    input_->set_text(":ask ");
    input_->set_placeholder("Ask local tools (layers / extent / fps / diag)…");
    input_->request_focus();
    return;
  }
  std::string line = cur;
  if (line.rfind(":ask", 0) != 0) {
    line = std::string(":ask ") + cur;
  }
  input_->set_text(line);
  on_submit();
}

void DebugConsolePanel::history_push(const std::string& line) {
  if (line.empty()) {
    history_index_ = -1;
    history_draft_.clear();
    return;
  }
  if (!history_.empty() && history_.back() == line) {
    history_index_ = -1;
    history_draft_.clear();
    return;
  }
  history_.push_back(line);
  if (history_.size() > static_cast<size_t>(kMaxHistory)) {
    history_.erase(history_.begin());
  }
  history_index_ = -1;
  history_draft_.clear();
}

bool DebugConsolePanel::history_navigate(int delta) {
  if (!input_ || history_.empty()) {
    return false;
  }
  if (history_index_ < 0) {
    history_draft_ = input_->text();
    history_index_ = static_cast<int>(history_.size());
  }
  const int next = history_index_ + delta;
  if (next < 0) {
    return true;
  }
  if (next >= static_cast<int>(history_.size())) {
    history_index_ = -1;
    input_->set_text(history_draft_);
    return true;
  }
  history_index_ = next;
  input_->set_text(history_[static_cast<size_t>(history_index_)]);
  return true;
}

bool DebugConsolePanel::try_tab_complete() {
  if (!input_) {
    return false;
  }
  const std::string cur = input_->text();
  if (cur.empty() || cur[0] != ':') {
    return false;
  }
  std::vector<std::string> matches;
  for (const char* prefix : kCmdPrefixes) {
    const std::string p(prefix);
    if (p.rfind(cur, 0) == 0) {
      matches.push_back(p);
    }
  }
  if (matches.empty()) {
    return true;
  }
  if (matches.size() == 1) {
    input_->set_text(matches[0]);
    return true;
  }
  std::string common = matches[0];
  for (size_t i = 1; i < matches.size(); ++i) {
    size_t n = 0;
    while (n < common.size() && n < matches[i].size() &&
           common[n] == matches[i][n]) {
      ++n;
    }
    common.resize(n);
  }
  if (common.size() > cur.size()) {
    input_->set_text(common);
  } else {
    std::string listed;
    for (size_t i = 0; i < matches.size(); ++i) {
      if (i) {
        listed += " | ";
      }
      listed += matches[i];
    }
    append_line("completions: " + listed);
  }
  return true;
}

bool DebugConsolePanel::on_input_key(const KeyEvent& event) {
  if (event.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (event.vk == VK_UP) {
    return history_navigate(-1);
  }
  if (event.vk == VK_DOWN) {
    return history_navigate(+1);
  }
  if (event.vk == VK_TAB) {
    return try_tab_complete();
  }
  return false;
}

void DebugConsolePanel::on_clear_clicked() {
  clear_output();
}

void DebugConsolePanel::on_copy_clicked() {
  std::string blob;
  if (selected_visible_ >= 0 &&
      selected_visible_ < static_cast<int>(visible_indices_.size())) {
    const size_t li =
        visible_indices_[static_cast<size_t>(selected_visible_)];
    if (li < lines_.size()) {
      blob = format_line_plain(lines_[li]);
    }
  } else {
    for (size_t vi : visible_indices_) {
      if (vi >= lines_.size()) {
        continue;
      }
      if (!blob.empty()) {
        blob.push_back('\n');
      }
      blob += format_line_plain(lines_[vi]);
    }
  }
  copy_utf8_to_clipboard(blob);
}

void DebugConsolePanel::on_filter_changed() {
  if (filter_) {
    filter_text_ = filter_->text();
  }
  selected_visible_ = -1;
  rebuild_visible_indices();
  sync_list_size(false);
}

void DebugConsolePanel::reload_from_sink() {
  const auto tail = base::log_sink().snapshot_tail(400);
  lines_.clear();
  lines_.reserve(tail.size());
  for (const auto& e : tail) {
    LogLine line;
    line.level = e.level;
    line.timestamp = e.timestamp;
    line.message = e.message;
    lines_.push_back(std::move(line));
  }
  rebuild_visible_indices();
  sync_list_size(true);
}

void DebugConsolePanel::rebuild_visible_indices() {
  visible_indices_.clear();
  visible_indices_.reserve(lines_.size());
  for (size_t i = 0; i < lines_.size(); ++i) {
    if (line_passes_filters(lines_[i])) {
      visible_indices_.push_back(i);
    }
  }
  if (count_) {
    count_->set_text(std::format("{}/{}", visible_indices_.size(), lines_.size()));
    count_->set_color(Theme::current().text_muted);
  }
}

bool DebugConsolePanel::line_passes_filters(const LogLine& line) const {
  switch (line.level) {
    case base::LogLevel::kFatal:
    case base::LogLevel::kError:
      if (show_error_ && !show_error_->is_checked()) {
        return false;
      }
      break;
    case base::LogLevel::kWarning:
      if (show_warn_ && !show_warn_->is_checked()) {
        return false;
      }
      break;
    case base::LogLevel::kNotice:
    case base::LogLevel::kInfo:
      if (show_info_ && !show_info_->is_checked()) {
        return false;
      }
      break;
    case base::LogLevel::kDebug:
    case base::LogLevel::kTrace:
      if (show_debug_ && !show_debug_->is_checked()) {
        return false;
      }
      break;
  }
  if (!filter_text_.empty() &&
      !contains_ci(line.message, filter_text_) &&
      !contains_ci(line.timestamp, filter_text_) &&
      !contains_ci(level_tag(line.level), filter_text_)) {
    return false;
  }
  return true;
}

void DebugConsolePanel::sync_list_size(bool stick_to_bottom) {
  if (!list_) {
    return;
  }
  const int rh = row_height();
  const int h =
      std::max(rh, static_cast<int>(visible_indices_.size()) * rh);
  int w = list_->preferred_size().width;
  if (scroll_ && scroll_->bounds().width > 0) {
    w = scroll_->bounds().width;
  } else if (list_->bounds().width > 0) {
    w = list_->bounds().width;
  }
  list_->set_preferred_size({w, h});
  if (scroll_) {
    scroll_->layout();
    if (stick_to_bottom && auto_scroll_) {
      scroll_to_bottom();
    }
  }
  if (list_) {
    list_->schedule_paint();
  }
  schedule_paint();
}

void DebugConsolePanel::scroll_to_bottom() {
  if (!scroll_ || !list_) {
    return;
  }
  scroll_->set_scroll_offset(list_->preferred_size().height);
}

std::string DebugConsolePanel::format_line_plain(const LogLine& line) const {
  if (line.timestamp.empty()) {
    return std::format("{} {}", level_tag(line.level), line.message);
  }
  return std::format("{} {} {}", line.timestamp, level_tag(line.level),
                     line.message);
}

float DebugConsolePanel::scale_factor() const {
  if (widget()) {
    return widget()->device_scale_factor();
  }
  return 1.f;
}

int DebugConsolePanel::row_height() const {
  return dip_to_px(kRowHeightDip, scale_factor());
}

void DebugConsolePanel::ensure_log_subscription() {
  if (log_sub_id_ || mode_ == PaneMode::kConsole) {
    return;
  }
  log_sub_id_ = base::log_sink().subscribe([this](const base::LogEntry& e) {
    LogLine line;
    line.level = e.level;
    line.timestamp = e.timestamp;
    line.message = e.message;
    enqueue_log_line(std::move(line));
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
