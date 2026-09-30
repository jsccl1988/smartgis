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
#include "ui/views/dialogs/file_picker.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/debug/render_trace_panel.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace {

class MemoryPageView : public View {
 public:
  void set_stats_label(Label* label) { stats_ = label; }

  void refresh(bool schedule = true) {
    events_.clear();
    base::trace::for_each_process_trace_event(
        [](void* ctx, int tid, base::trace::Trace::time_point begin,
           base::trace::Trace::time_point end, const char* name, const char* cat,
           base::trace::Trace::Event::Kind kind, int64_t counter_value) {
          auto* events = static_cast<std::vector<base::trace::Trace::Event>*>(ctx);
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
    const int top = b.y + 28;
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
  Label* stats_ = nullptr;
  std::vector<base::trace::Trace::Event> events_;
  base::trace::Trace::time_point origin_{};
};

}  // namespace

DiagnosticToolsPanel::DiagnosticToolsPanel() {
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  box->set_inside_border(6, 4, 6, 4);
  box->set_between_child_spacing(4);

  auto title = std::make_unique<Label>("Diagnostic Tools");
  title->set_preferred_size({360, 20});
  title_ = title.get();

  auto status = std::make_unique<Label>("Idle");
  status->set_preferred_size({480, 18});
  status_ = status.get();

  auto row = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  row->set_between_child_spacing(4);

  auto record = std::make_unique<Button>("Record");
  record->set_preferred_size({72, 26});
  record_ = record.get();
  record_->set_click([this] { on_record(); });

  auto stop = std::make_unique<Button>("Stop");
  stop->set_preferred_size({64, 26});
  stop_ = stop.get();
  stop_->set_click([this] { on_stop(); });

  auto clear = std::make_unique<Button>("Clear");
  clear->set_preferred_size({64, 26});
  clear_ = clear.get();
  clear_->set_click([this] { on_clear(); });

  auto exp = std::make_unique<Button>("Export");
  exp->set_preferred_size({72, 26});
  export_ = exp.get();
  export_->set_click([this] { on_export(); });

  auto refresh = std::make_unique<Button>("Refresh");
  refresh->set_preferred_size({72, 26});
  refresh_ = refresh.get();
  refresh_->set_click([this] { on_refresh(); });

  auto arm = std::make_unique<Checkbox>("Armed");
  arm->set_preferred_size({80, 24});
  arm_ = arm.get();
  arm_->set_change([this](bool on) {
    base::trace::set_tracing_enabled(on);
    update_status();
  });

  auto track = std::make_unique<Checkbox>("Track allocs");
  track->set_preferred_size({110, 24});
  track_allocs_ = track.get();
  track_allocs_->set_change([](bool on) {
    if (on) {
      base::AllocationTracker::enable();
    } else {
      base::AllocationTracker::disable();
    }
  });

  auto echo = std::make_unique<Checkbox>("Echo→Output");
  echo->set_preferred_size({110, 24});
  echo->set_checked(true);
  echo_commands_ = echo.get();

  auto toolbar = std::make_unique<View>();
  toolbar->set_layout_manager(std::move(row));
  toolbar->set_preferred_size({640, 30});
  toolbar->add_child(std::move(record));
  toolbar->add_child(std::move(stop));
  toolbar->add_child(std::move(clear));
  toolbar->add_child(std::move(exp));
  toolbar->add_child(std::move(refresh));
  toolbar->add_child(std::move(arm));
  toolbar->add_child(std::move(track));
  toolbar->add_child(std::move(echo));

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

  auto mem_stats = std::make_unique<Label>("Memory: —");
  mem_stats->set_preferred_size({480, 40});
  memory_stats_ = mem_stats.get();

  auto memory = std::make_unique<MemoryPageView>();
  memory->set_stats_label(memory_stats_);
  memory->set_preferred_size({0, 120});
  memory_page_ = memory.get();

  auto mem_host = std::make_unique<View>();
  auto mem_box =
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  mem_box->set_between_child_spacing(4);
  mem_box->set_flex_for_view(memory.get(), 1);
  mem_host->set_layout_manager(std::move(mem_box));
  mem_host->add_child(std::move(mem_stats));
  mem_host->add_child(std::move(memory));

  auto tabs = std::make_unique<TabStrip>();
  tabs->set_preferred_size({0, 140});
  tabs_ = tabs.get();
  tabs_->add_tab("Output", std::move(output));
  tabs_->add_tab("Console", std::move(console));
  tabs_->add_tab("CPU", std::move(cpu));
  tabs_->add_tab("Memory", std::move(mem_host));

  box->set_flex_for_view(tabs.get(), 1);
  set_layout_manager(std::move(box));
  add_child(std::move(title));
  add_child(std::move(status));
  add_child(std::move(toolbar));
  add_child(std::move(tabs));
  set_preferred_size({0, 0});

  // Always-on diagnostics: reflect process state (do not re-enable / clear).
  if (base::trace::tracing_enabled()) {
    arm_->set_checked(true);
  }
  if (base::AllocationTracker::is_enabled()) {
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
  set_preferred_size(on ? Size{0, 220} : Size{0, 0});
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
      "{} | events={} | Output/Console/CPU/Memory (auto-refresh)",
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
