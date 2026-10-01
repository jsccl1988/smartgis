// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/debug/render_trace_panel.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "base/trace/event/trace.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {
namespace {

ui::gfx::Color lane_color(std::size_t i) {
  static const ui::gfx::Color kColors[] = {
      0xff4c8bf5u, 0xff3db88cu, 0xffe6a23cu, 0xfff56c6cu,
      0xff909399u, 0xff9b59b6u, 0xff1abc9cu, 0xffe67e22u};
  return kColors[i % (sizeof(kColors) / sizeof(kColors[0]))];
}

bool cat_is_map2d(std::string_view cat, std::string_view name) {
  return cat.starts_with("map2d") || name.starts_with("map2d");
}

bool cat_is_scene3d(std::string_view cat, std::string_view name) {
  return cat.starts_with("scene3d") || name.starts_with("scene3d");
}

bool cat_is_startup(std::string_view cat, std::string_view name) {
  return cat.starts_with("startup") || name.starts_with("startup");
}

bool cat_is_gdi(std::string_view cat, std::string_view name) {
  return cat.starts_with("gdi") || name.starts_with("gdi");
}

bool cat_is_ui_views(std::string_view cat, std::string_view name) {
  return cat.starts_with("ui.views") || cat.starts_with("ui.") ||
         name.starts_with("ui.views") || name == "on_paint" ||
         name == "record_commit" || name == "blt_present" || name == "raster";
}

}  // namespace

struct RenderTracePanel::State {
  std::vector<base::trace::Trace::Event> events;
  std::vector<base::trace::TracePhaseRollup> phases;
  base::trace::Trace::time_point origin{};
};

RenderTracePanel::RenderTracePanel() : state_(std::make_unique<State>()) {
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  box->set_inside_border(8, 6, 8, 6);
  box->set_between_child_spacing(4);

  auto title =
      std::make_unique<Label>(
          "Perf Gantt (UI Views + Map2d + Scene3d + GDI + Startup)");
  title->set_preferred_size({520, 22});
  title_ = title.get();

  auto status =
      std::make_unique<Label>("Always-on �?open Diagnostic Tools to watch");
  status->set_preferred_size({480, 20});
  status_ = status.get();

  auto row = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  row->set_between_child_spacing(4);

  auto record = std::make_unique<Button>("Record");
  record->set_preferred_size({72, 28});
  record_ = record.get();
  record_->set_click([this]() { on_record(); });

  auto stop = std::make_unique<Button>("Stop");
  stop->set_preferred_size({64, 28});
  stop_ = stop.get();
  stop_->set_click([this]() { on_stop(); });

  auto clear = std::make_unique<Button>("Clear");
  clear->set_preferred_size({64, 28});
  clear_ = clear.get();
  clear_->set_click([this]() { on_clear(); });

  auto exp = std::make_unique<Button>("Export");
  exp->set_preferred_size({72, 28});
  export_ = exp.get();
  export_->set_click([this]() { on_export(); });

  auto refresh = std::make_unique<Button>("Refresh");
  refresh->set_preferred_size({72, 28});
  refresh_ = refresh.get();
  refresh_->set_click([this]() { on_refresh(); });

  auto arm = std::make_unique<Checkbox>("Armed");
  arm->set_preferred_size({80, 24});
  arm_ = arm.get();
  arm_->set_change([this](bool on) {
    base::trace::set_tracing_enabled(on);
    update_status();
  });

  auto toolbar = std::make_unique<View>();
  toolbar->set_layout_manager(std::move(row));
  toolbar->set_preferred_size({480, 32});
  toolbar->add_child(std::move(record));
  toolbar->add_child(std::move(stop));
  toolbar->add_child(std::move(clear));
  toolbar->add_child(std::move(exp));
  toolbar->add_child(std::move(refresh));
  toolbar->add_child(std::move(arm));

  auto filt = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  filt->set_between_child_spacing(8);
  auto show2d = std::make_unique<Checkbox>("Map2d");
  show2d->set_preferred_size({90, 24});
  show2d->set_checked(true);
  show_map2d_ = show2d.get();
  show_map2d_->set_change([this](bool) {
    refresh_from_process_trace();
    update_status();
  });
  auto show3d = std::make_unique<Checkbox>("Scene3d");
  show3d->set_preferred_size({100, 24});
  show3d->set_checked(true);
  show_scene3d_ = show3d.get();
  show_scene3d_->set_change([this](bool) {
    refresh_from_process_trace();
    update_status();
  });
  auto show_startup = std::make_unique<Checkbox>("Startup");
  show_startup->set_preferred_size({90, 24});
  show_startup->set_checked(true);
  show_startup_ = show_startup.get();
  show_startup_->set_change([this](bool) {
    refresh_from_process_trace();
    update_status();
  });
  auto show_gdi = std::make_unique<Checkbox>("GDI");
  show_gdi->set_preferred_size({70, 24});
  show_gdi->set_checked(true);
  show_gdi_ = show_gdi.get();
  show_gdi_->set_change([this](bool) {
    refresh_from_process_trace();
    update_status();
  });
  auto show_ui = std::make_unique<Checkbox>("UI");
  show_ui->set_preferred_size({60, 24});
  show_ui->set_checked(true);
  show_ui_ = show_ui.get();
  show_ui_->set_change([this](bool) {
    refresh_from_process_trace();
    update_status();
  });
  auto filters = std::make_unique<View>();
  filters->set_layout_manager(std::move(filt));
  filters->set_preferred_size({560, 28});
  filters->add_child(std::move(show2d));
  filters->add_child(std::move(show3d));
  filters->add_child(std::move(show_startup));
  filters->add_child(std::move(show_gdi));
  filters->add_child(std::move(show_ui));

  auto rollup = std::make_unique<Label>("");
  rollup->set_preferred_size({480, 56});
  rollup_ = rollup.get();

  set_layout_manager(std::move(box));
  add_child(std::move(title));
  add_child(std::move(status));
  add_child(std::move(toolbar));
  add_child(std::move(filters));
  add_child(std::move(rollup));
  set_preferred_size({500, 260});

  if (base::trace::tracing_enabled()) {
    arm_->set_checked(true);
  }
  update_status();
}

RenderTracePanel::~RenderTracePanel() {
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
  if (show_map2d_) {
    show_map2d_->set_change({});
  }
  if (show_scene3d_) {
    show_scene3d_->set_change({});
  }
  if (show_startup_) {
    show_startup_->set_change({});
  }
  if (show_gdi_) {
    show_gdi_->set_change({});
  }
  remove_all_children();
  title_ = nullptr;
  status_ = nullptr;
  rollup_ = nullptr;
  record_ = nullptr;
  stop_ = nullptr;
  clear_ = nullptr;
  export_ = nullptr;
  refresh_ = nullptr;
  arm_ = nullptr;
  show_map2d_ = nullptr;
  show_scene3d_ = nullptr;
  show_startup_ = nullptr;
  show_gdi_ = nullptr;
  state_.reset();
}

std::unique_ptr<RenderTracePanel> make_render_trace_panel() {
  return std::make_unique<RenderTracePanel>();
}

void RenderTracePanel::set_embedded(bool embedded) {
  embedded_ = embedded;
  // DiagnosticToolsPanel already draws Record/status; collapse local chrome so
  // the gantt lane band owns the page height (avoids label pile-up at +150).
  const Size chrome = embedded ? Size{0, 0} : Size{480, 22};
  const Size status = embedded ? Size{0, 0} : Size{480, 20};
  const Size toolbar = embedded ? Size{0, 0} : Size{480, 32};
  const Size filters = embedded ? Size{0, 0} : Size{480, 28};
  const Size rollup = embedded ? Size{0, 0} : Size{480, 56};
  if (title_) {
    title_->set_preferred_size(chrome);
    title_->set_visible(!embedded);
  }
  if (status_) {
    status_->set_preferred_size(status);
    status_->set_visible(!embedded);
  }
  if (rollup_) {
    rollup_->set_preferred_size(rollup);
    rollup_->set_visible(!embedded);
  }
  // Toolbar / filters are anonymous Views �?walk children by preferred size.
  for (size_t i = 0; i < child_count(); ++i) {
    View* c = child_at(i);
    if (!c || c == title_ || c == status_ || c == rollup_) {
      continue;
    }
    if (c->preferred_size().height == 32 || c->preferred_size().height == 30) {
      c->set_preferred_size(toolbar);
      c->set_visible(!embedded);
    } else if (c->preferred_size().height == 28) {
      c->set_preferred_size(filters);
      c->set_visible(!embedded);
    }
  }
  set_preferred_size(embedded ? Size{0, 120} : Size{500, 260});
  layout();
}

void RenderTracePanel::on_device_scale_factor_changed(float old_scale,
                                                     float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
}

namespace {

std::vector<base::trace::Trace::Event> visible_events(
    const RenderTracePanel::State& state,
    const Checkbox* show_map2d,
    const Checkbox* show_scene3d,
    const Checkbox* show_startup,
    const Checkbox* show_gdi,
    const Checkbox* show_ui) {
  std::vector<base::trace::Trace::Event> out;
  out.reserve(state.events.size());
  for (const auto& e : state.events) {
    if (e.kind == base::trace::Trace::Event::Kind::kCounter) {
      continue;
    }
    const bool is2d = cat_is_map2d(e.cat, e.name);
    const bool is3d = cat_is_scene3d(e.cat, e.name);
    const bool is_startup = cat_is_startup(e.cat, e.name);
    const bool is_gdi = cat_is_gdi(e.cat, e.name);
    const bool is_ui = cat_is_ui_views(e.cat, e.name);
    const bool want2d = !show_map2d || show_map2d->is_checked();
    const bool want3d = !show_scene3d || show_scene3d->is_checked();
    const bool want_startup = !show_startup || show_startup->is_checked();
    const bool want_gdi = !show_gdi || show_gdi->is_checked();
    const bool want_ui = !show_ui || show_ui->is_checked();
    if (is2d && !want2d) {
      continue;
    }
    if (is3d && !want3d) {
      continue;
    }
    if (is_startup && !want_startup) {
      continue;
    }
    if (is_gdi && !want_gdi) {
      continue;
    }
    if (is_ui && !want_ui) {
      continue;
    }
    if (!is2d && !is3d && !is_startup && !is_gdi && !is_ui &&
        !(want2d || want3d || want_startup || want_gdi || want_ui)) {
      continue;
    }
    out.push_back(e);
  }
  return out;
}

}  // namespace

void RenderTracePanel::on_record() {
  base::trace::process_trace().clear();
  base::trace::set_tracing_enabled(true);
  if (arm_) {
    arm_->set_checked(true);
  }
  update_status();
}

void RenderTracePanel::on_stop() {
  base::trace::set_tracing_enabled(false);
  if (arm_) {
    arm_->set_checked(false);
  }
  refresh_from_process_trace();
  update_status();
}

void RenderTracePanel::on_clear() {
  base::trace::process_trace().clear();
  if (state_) {
    state_->events.clear();
    state_->phases.clear();
  }
  if (rollup_) {
    rollup_->set_text("");
  }
  schedule_paint();
  update_status();
}

void RenderTracePanel::on_export() {
  refresh_from_process_trace();
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

void RenderTracePanel::on_refresh() {
  refresh_from_process_trace();
  update_status();
}

void RenderTracePanel::refresh_from_process_trace(bool schedule) {
  if (!state_) {
    return;
  }
  // Copy events via DLL-safe callback �?never assign snapshot_events() across
  // DLL boundaries (MSVC debug iterator / vector ABI).
  state_->events.clear();
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
      &state_->events);
  state_->origin = base::trace::process_trace_origin();
  const auto vis =
      visible_events(*state_, show_map2d_, show_scene3d_, show_startup_,
                     show_gdi_, show_ui_);
  state_->phases = base::trace::rollup_trace_phases(vis);
  if (rollup_) {
    std::string text;
    const size_t n = (std::min)(state_->phases.size(), size_t{6});
    for (size_t i = 0; i < n; ++i) {
      const auto& p = state_->phases[i];
      text += std::format("{} n={} avg={:.0f}us p99={}\n", p.name, p.count,
                          p.avg_us, p.p99_us);
    }
    if (state_->phases.size() > n) {
      text += std::format("�?+{} phases\n", state_->phases.size() - n);
    }
    rollup_->set_text(text);
  }
  if (schedule) {
    schedule_paint();
  }
}

void RenderTracePanel::update_status() {
  if (!status_ || !state_) {
    return;
  }
  size_t n2 = 0;
  size_t n3 = 0;
  size_t n_startup = 0;
  for (const auto& e : state_->events) {
    if (e.kind == base::trace::Trace::Event::Kind::kCounter) {
      continue;
    }
    if (cat_is_map2d(e.cat, e.name)) {
      ++n2;
    }
    if (cat_is_scene3d(e.cat, e.name)) {
      ++n3;
    }
    if (cat_is_startup(e.cat, e.name)) {
      ++n_startup;
    }
  }
  status_->set_text(std::format(
      "{} | total={} startup={} map2d={} scene3d={}",
      base::trace::tracing_enabled() ? "Recording" : "Stopped",
      base::trace::process_trace().size(), n_startup, n2, n3));
}

void RenderTracePanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas || !state_) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  // Prefer remaining height under chrome; embedded mode collapses chrome so
  // lane_top sits just below panel top (+ inset).
  int chrome_bottom = b.y + 8;
  if (!embedded_) {
    chrome_bottom = b.y + 150;
  }
  const auto vis =
      visible_events(*state_, show_map2d_, show_scene3d_, show_startup_,
                     show_gdi_, show_ui_);
  if (vis.empty()) {
    canvas->draw_text(b.x + 8, chrome_bottom + 4,
                      L"No duration events yet � Record or wait for refresh",
                      t.text_muted);
    return;
  }

  std::unordered_map<std::string, std::size_t> lane_of;
  std::vector<std::string> lanes;
  auto lane_key = [](const base::trace::Trace::Event& e) {
    return e.cat.empty() ? e.name : e.cat;
  };
  for (const auto& e : vis) {
    const std::string key = lane_key(e);
    if (lane_of.find(key) == lane_of.end()) {
      lane_of.emplace(key, lanes.size());
      lanes.push_back(key);
    }
  }
  const int lane_bottom = b.bottom() - 6;
  const int lane_top = chrome_bottom;
  if (lane_bottom <= lane_top + 8 || lanes.empty()) {
    return;
  }
  const int lane_h =
      (std::max)(14, (lane_bottom - lane_top) /
                         (std::max)(1, static_cast<int>(lanes.size())));

  canvas->fill_rect(b.x + 4, lane_top, b.width - 8, lane_bottom - lane_top,
                    t.panel_bg);

  int64_t min_ts = 0;
  int64_t max_ts = 1;
  bool first = true;
  for (const auto& e : vis) {
    const auto ts = std::chrono::duration_cast<base::trace::Trace::duration>(
                        e.begin - state_->origin)
                        .count();
    const auto dur =
        std::chrono::duration_cast<base::trace::Trace::duration>(e.end - e.begin)
            .count();
    if (first) {
      min_ts = ts;
      max_ts = ts + (std::max)(dur, int64_t{1});
      first = false;
    } else {
      min_ts = (std::min)(min_ts, ts);
      max_ts = (std::max)(max_ts, ts + (std::max)(dur, int64_t{1}));
    }
  }
  const double span =
      static_cast<double>((std::max)(max_ts - min_ts, int64_t{1}));

  const int left = b.x + 88;
  const int right = b.right() - 8;
  const int width = (std::max)(1, right - left);

  for (std::size_t i = 0; i < lanes.size(); ++i) {
    const int y = lane_top + static_cast<int>(i) * lane_h;
    canvas->draw_text(b.x + 6, y + 2, utf8_to_wide(lanes[i]).c_str(),
                      t.text_muted);
    canvas->fill_rect(left, y + lane_h - 1, width, 1, t.text_muted);
  }

  for (const auto& e : vis) {
    const auto ts = std::chrono::duration_cast<base::trace::Trace::duration>(
                        e.begin - state_->origin)
                        .count();
    const auto dur =
        std::chrono::duration_cast<base::trace::Trace::duration>(e.end - e.begin)
            .count();
    const std::size_t li = lane_of[lane_key(e)];
    const int y = lane_top + static_cast<int>(li) * lane_h + 2;
    const int x0 =
        left + static_cast<int>((static_cast<double>(ts - min_ts) / span) *
                                width);
    const int w = (std::max)(
        2, static_cast<int>((static_cast<double>((std::max)(dur, int64_t{1})) /
                             span) *
                            width));
    canvas->fill_rect(x0, y, w, lane_h - 4, lane_color(li));
  }
}

}  // namespace views
}  // namespace ui
