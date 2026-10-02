// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/debug/diagnostic_tools_panel.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "base/memory/allocation_tracker.h"
#include "base/memory/arena.h"
#include "base/memory/sample_trace.h"
#include "base/trace/event/process_trace.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/debug/render_trace_panel.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {
namespace {

// Memory counter sparkline; page chrome (stats + host) is markup.
class MemoryChartView : public View {
 public:
  void refresh(bool schedule = true) {
    events_.clear();
    base::trace::for_each_process_trace_event(
        [](void* ctx, int tid, base::trace::Trace::time_point begin,
           base::trace::Trace::time_point end, const char* name, const char* cat,
           base::trace::Trace::Event::Kind kind, int64_t counter_value) {
          auto* events =
              static_cast<std::vector<base::trace::Trace::Event>*>(ctx);
          base::trace::Trace::Event ev;
          ev.tid = tid;
          ev.begin = begin;
          ev.end = end;
          ev.name = name ? name : "";
          ev.cat = cat ? cat : "";
          ev.kind = kind;
          ev.counter_value = counter_value;
          events->push_back(std::move(ev));
        },
        &events_);
    origin_ = base::trace::process_trace_origin();
    if (schedule) {
      schedule_paint();
    }
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);

    std::vector<const base::trace::Trace::Event*> samples;
    for (const auto& e : events_) {
      if (e.kind == base::trace::Trace::Event::Kind::kCounter &&
          e.cat.find("memory") != std::string::npos) {
        samples.push_back(&e);
      }
    }
    if (samples.size() < 2) {
      canvas->draw_text(b.x + 8, b.y + 8,
                        L"Waiting for memory samples (always-on 500ms)",
                        t.text_muted);
      return;
    }

    int64_t min_ts = 0;
    int64_t max_ts = 1;
    int64_t max_v = 1;
    bool first = true;
    for (const auto* e : samples) {
      const auto ts = std::chrono::duration_cast<base::trace::Trace::duration>(
                          e->begin - origin_)
                          .count();
      if (first) {
        min_ts = ts;
        max_ts = ts;
        max_v = (std::max)(e->counter_value, int64_t{1});
        first = false;
      } else {
        min_ts = (std::min)(min_ts, ts);
        max_ts = (std::max)(max_ts, ts);
        max_v = (std::max)(max_v, e->counter_value);
      }
    }
    const double span =
        static_cast<double>((std::max)(max_ts - min_ts, int64_t{1}));
    const int left = b.x + 8;
    const int right = b.right() - 8;
    const int top = b.y + 8;
    const int bottom = b.bottom() - 8;
    const int width = (std::max)(1, right - left);
    const int height = (std::max)(1, bottom - top);
    canvas->fill_rect(left, top, width, height, t.control_bg);

    int prev_x = left;
    int prev_y = bottom;
    bool have_prev = false;
    for (const auto* e : samples) {
      if (e->name != "process_used" && e->name != "tls_used" &&
          e->name != "tracker_live") {
        continue;
      }
      const auto ts = std::chrono::duration_cast<base::trace::Trace::duration>(
                          e->begin - origin_)
                          .count();
      const int x =
          left + static_cast<int>((static_cast<double>(ts - min_ts) / span) *
                                  width);
      const int y =
          bottom - static_cast<int>((static_cast<double>(e->counter_value) /
                                    static_cast<double>(max_v)) *
                                   height);
      if (have_prev) {
        canvas->fill_rect((std::min)(prev_x, x), (std::min)(prev_y, y),
                          (std::max)(2, std::abs(x - prev_x)),
                          (std::max)(2, std::abs(y - prev_y)), 0xff4c8bf5u);
      }
      canvas->fill_rect(x - 1, y - 1, 3, 3, 0xff3db88cu);
      prev_x = x;
      prev_y = y;
      have_prev = true;
    }
  }

 private:
  std::vector<base::trace::Trace::Event> events_;
  base::trace::Trace::time_point origin_{};
};

// Memory tab page: markup stats row + chart host; chart paint stays C++.
class MemoryPageView : public View {
 public:
  MemoryPageView() {
    MarkupRoot loaded = load_markup("debug/memory_page.ui.xml");
    if (!loaded.ok()) {
      set_preferred_size({0, 140});
      return;
    }
    stats_ = loaded.ids.find_as<Label>("stats");
    View* chart_host = loaded.ids.find("chart_host");

    auto chart = std::make_unique<MemoryChartView>();
    chart_ = chart.get();
    if (chart_host) {
      chart_host->set_layout_manager(std::make_unique<FillLayout>());
      chart_host->add_child(std::move(chart));
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({0, 140});
    add_child(std::move(loaded.root));
    set_preferred_size({0, 140});
  }

  Label* stats_label() { return stats_; }

  void refresh(bool schedule = true) {
    if (chart_) {
      chart_->refresh(schedule);
    }
  }

 private:
  Label* stats_ = nullptr;
  MemoryChartView* chart_ = nullptr;
};

}  // namespace

DiagnosticToolsPanel::DiagnosticToolsPanel() {
  MarkupRoot loaded = load_markup("debug/diagnostic_tools_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({0, 0});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  status_ = loaded.ids.find_as<Label>("status");
  record_ = loaded.ids.find_as<Button>("record");
  stop_ = loaded.ids.find_as<Button>("stop");
  clear_ = loaded.ids.find_as<Button>("clear");
  export_ = loaded.ids.find_as<Button>("export");
  refresh_ = loaded.ids.find_as<Button>("refresh");
  arm_ = loaded.ids.find_as<Checkbox>("arm");
  track_allocs_ = loaded.ids.find_as<Checkbox>("track_allocs");
  echo_commands_ = loaded.ids.find_as<Checkbox>("echo_commands");
  View* tabs_host = loaded.ids.find("tabs_host");

  if (record_) {
    record_->set_click([this] { on_record(); });
  }
  if (stop_) {
    stop_->set_click([this] { on_stop(); });
  }
  if (clear_) {
    clear_->set_click([this] { on_clear(); });
  }
  if (export_) {
    export_->set_click([this] { on_export(); });
  }
  if (refresh_) {
    refresh_->set_click([this] { on_refresh(); });
  }
  if (arm_) {
    arm_->set_change([this](bool on) {
      base::trace::set_tracing_enabled(on);
      update_status();
    });
  }
  if (track_allocs_) {
    track_allocs_->set_change([](bool on) {
      if (on) {
        base::AllocationTracker::enable();
      } else {
        base::AllocationTracker::disable();
      }
    });
  }

  auto output = std::make_unique<DebugConsolePanel>();
  output->set_pane_mode(DebugConsolePanel::PaneMode::kOutput);
  output->set_visible_console(true);
  output_ = output.get();

  auto console = std::make_unique<DebugConsolePanel>();
  console->set_pane_mode(DebugConsolePanel::PaneMode::kConsole);
  console->set_visible_console(true);
  console_ = console.get();
  console_->set_echo_to_output([this](const std::string& line) {
    if (echo_commands_ && echo_commands_->is_checked() && output_) {
      output_->append_line(line);
    }
  });

  auto cpu = std::make_unique<RenderTracePanel>();
  cpu->set_embedded(true);
  cpu_ = cpu.get();

  auto memory = std::make_unique<MemoryPageView>();
  memory_stats_ = memory->stats_label();
  memory_page_ = memory.get();

  auto tabs = std::make_unique<TabStrip>();
  tabs->set_preferred_size({0, 140});
  tabs_ = tabs.get();
  tabs_->add_tab("Output", std::move(output));
  tabs_->add_tab("Console", std::move(console));
  tabs_->add_tab("Trace", std::move(cpu));
  tabs_->add_tab("Memory", std::move(memory));

  if (tabs_host) {
    tabs_host->set_preferred_size({0, 140});
    tabs_host->set_layout_manager(std::make_unique<FillLayout>());
    tabs_host->add_child(std::move(tabs));
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({0, 240});
  add_child(std::move(loaded.root));
  set_preferred_size({0, 0});

  // Always-on diagnostics: reflect process state (do not re-enable / clear).
  if (arm_ && base::trace::tracing_enabled()) {
    arm_->set_checked(true);
  }
  if (track_allocs_ && base::AllocationTracker::is_enabled()) {
    track_allocs_->set_checked(true);
  }
  last_auto_refresh_ = std::chrono::steady_clock::now();
  update_status();
  on_refresh();
  // Start collapsed: hide chrome children so a thin splitter remnant cannot
  // paint "Diagnostic Tools" / tabs into the status-bar band.
  for (size_t i = 0; i < child_count(); ++i) {
    if (View* c = child_at(i)) {
      c->set_visible(false);
    }
  }
}

DiagnosticToolsPanel::~DiagnosticToolsPanel() {
  if (record_) {
    record_->set_click({});
  }
  if (stop_) {
    stop_->set_click({});
  }
  if (clear_) {
    clear_->set_click({});
  }
  if (export_) {
    export_->set_click({});
  }
  if (refresh_) {
    refresh_->set_click({});
  }
  if (arm_) {
    arm_->set_change({});
  }
  if (track_allocs_) {
    track_allocs_->set_change({});
  }
  remove_all_children();
  title_ = nullptr;
  status_ = nullptr;
  memory_stats_ = nullptr;
  record_ = nullptr;
  stop_ = nullptr;
  clear_ = nullptr;
  export_ = nullptr;
  refresh_ = nullptr;
  arm_ = nullptr;
  track_allocs_ = nullptr;
  echo_commands_ = nullptr;
  tabs_ = nullptr;
  output_ = nullptr;
  console_ = nullptr;
  cpu_ = nullptr;
  memory_page_ = nullptr;
}

std::unique_ptr<DiagnosticToolsPanel> make_diagnostic_tools_panel() {
  return std::make_unique<DiagnosticToolsPanel>();
}

void DiagnosticToolsPanel::set_visible_tools(bool on) {
  if (visible_ == on) {
    return;
  }
  visible_ = on;
  set_preferred_size(on ? Size{0, 240} : Size{0, 0});
  // Hide chrome while collapsed so children cannot paint into a remnant strip.
  // Output keeps LogSink subscription even while collapsed so RHI / present
  // LOGGING still accumulates and snapshot_tail is not the only recovery path.
  for (size_t i = 0; i < child_count(); ++i) {
    if (View* c = child_at(i)) {
      c->set_visible(on);
    }
  }
  if (output_) {
    output_->set_visible_console(true);
  }
  if (console_) {
    console_->set_visible_console(on);
  }
  if (on) {
    on_refresh();
    last_auto_refresh_ = std::chrono::steady_clock::now();
  }
  // Parent is BrowserView's vertical main_split: reseed so preferred 0 does
  // not leave a stale half-height secondary, and preferred 220 pins tools.
  if (auto* split = dynamic_cast<Splitter*>(parent())) {
    split->reseed();
  } else if (View* p = parent()) {
    p->layout();
  } else {
    layout();
  }
  schedule_paint();
}

void DiagnosticToolsPanel::set_active_tab(int index) {
  if (!tabs_ || index < 0 || index >= tabs_->tab_count()) {
    return;
  }
  tabs_->set_active(index);
}

int DiagnosticToolsPanel::active_tab() const {
  return tabs_ ? tabs_->active() : -1;
}

void DiagnosticToolsPanel::set_console_submit(
    std::function<void(const std::string&)> fn) {
  if (console_) {
    console_->set_submit_handler(std::move(fn));
  }
}

void DiagnosticToolsPanel::on_device_scale_factor_changed(float old_scale,
                                                         float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
}

void DiagnosticToolsPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas || !visible_) {
    return;
  }
  // Do NOT schedule_paint here — that caused full-rate shell republish flicker.
  // Snapshot refresh is on Record/Stop/Clear/Refresh and when the user opens
  // tools (set_visible_tools).
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
}

void DiagnosticToolsPanel::on_record() {
  // Fresh capture window while keeping always-on mode armed.
  base::trace::process_trace().clear();
  base::trace::set_tracing_enabled(true);
  if (arm_) {
    arm_->set_checked(true);
  }
  sample_memory();
  update_status();
}

void DiagnosticToolsPanel::on_stop() {
  base::trace::set_tracing_enabled(false);
  if (arm_) {
    arm_->set_checked(false);
  }
  on_refresh();
  update_status();
}

void DiagnosticToolsPanel::on_clear() {
  base::trace::process_trace().clear();
  if (output_) {
    output_->clear_output();
  }
  if (console_) {
    console_->clear_output();
  }
  if (cpu_) {
    cpu_->refresh_from_process_trace();
  }
  if (auto* mem = static_cast<MemoryPageView*>(memory_page_)) {
    mem->refresh();
  }
  refresh_memory_stats();
  update_status();
  schedule_paint();
}

void DiagnosticToolsPanel::on_export() {
  sample_memory();
  if (cpu_) {
    cpu_->refresh_from_process_trace();
  }
  const FilePickerResult pick =
      pick_save_file(L"Chrome Trace JSON (*.json)\0*.json\0\0");
  if (!pick.accepted || pick.path.empty()) {
    return;
  }
  std::ofstream out(pick.path, std::ios::binary);
  if (!out) {
    return;
  }
  out << base::trace::process_trace().dump();
}

void DiagnosticToolsPanel::on_refresh() {
  sample_memory();
  if (cpu_) {
    cpu_->refresh_from_process_trace();
  }
  if (auto* mem = static_cast<MemoryPageView*>(memory_page_)) {
    mem->refresh();
  }
  refresh_memory_stats();
  update_status();
}

void DiagnosticToolsPanel::sample_memory() {
  base::sample_memory_counters_to_process_trace();
}

void DiagnosticToolsPanel::refresh_memory_stats() {
  if (!memory_stats_) {
    return;
  }
  size_t proc_used = 0;
  size_t proc_cap = 0;
  size_t tls_used = 0;
  if (base::MemoryResource* mr = base::memory_resource()) {
    proc_used = mr->used();
    proc_cap = mr->capacity();
  }
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls_used = tls->used();
  }
  memory_stats_->set_text(std::format(
      "process used={} / cap={} | tls used={} | tracker live={} peak={} "
      "(Track allocs {})",
      proc_used, proc_cap, tls_used, base::AllocationTracker::live_bytes(),
      base::AllocationTracker::peak_bytes(),
      base::AllocationTracker::is_enabled() ? "on" : "off"));
}

void DiagnosticToolsPanel::update_status() {
  if (!status_) {
    return;
  }
  status_->set_text(std::format(
      "{} | events={} | Output/Console/Trace/Memory (UI paint profile)",
      base::trace::tracing_enabled() ? "Recording" : "Stopped",
      base::trace::process_trace().size()));
}

void DiagnosticToolsPanel::maybe_auto_refresh() {
  if (!visible_) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  if (now - last_auto_refresh_ < std::chrono::milliseconds(500)) {
    return;
  }
  last_auto_refresh_ = now;
  if (cpu_) {
    cpu_->refresh_from_process_trace(/*schedule=*/false);
  }
  if (auto* mem = static_cast<MemoryPageView*>(memory_page_)) {
    mem->refresh(/*schedule=*/false);
  }
  refresh_memory_stats();
  update_status();
}

}  // namespace views
}  // namespace ui
