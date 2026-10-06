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
#include "ui/views/markup/loader/markup_loader.h"
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
  MarkupRoot loaded = load_markup("debug/render_trace_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({500, 260});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  status_ = loaded.ids.find_as<Label>("status");
  rollup_ = loaded.ids.find_as<Label>("rollup");
  toolbar_ = loaded.ids.find("toolbar");
  filters_ = loaded.ids.find("filters");
  record_ = loaded.ids.find_as<Button>("record");
  stop_ = loaded.ids.find_as<Button>("stop");
  clear_ = loaded.ids.find_as<Button>("clear");
  export_ = loaded.ids.find_as<Button>("export");
  refresh_ = loaded.ids.find_as<Button>("refresh");
  arm_ = loaded.ids.find_as<Checkbox>("arm");
  show_map2d_ = loaded.ids.find_as<Checkbox>("show_map2d");
  show_scene3d_ = loaded.ids.find_as<Checkbox>("show_scene3d");
  show_startup_ = loaded.ids.find_as<Checkbox>("show_startup");
  show_gdi_ = loaded.ids.find_as<Checkbox>("show_gdi");
  show_ui_ = loaded.ids.find_as<Checkbox>("show_ui");

  if (record_) {
    record_->set_click([this]() { on_record(); });
  }
  if (stop_) {
    stop_->set_click([this]() { on_stop(); });
  }
  if (clear_) {
    clear_->set_click([this]() { on_clear(); });
  }
  if (export_) {
    export_->set_click([this]() { on_export(); });
  }
  if (refresh_) {
    refresh_->set_click([this]() { on_refresh(); });
  }
  if (arm_) {
    arm_->set_change([this](bool on) {
      base::trace::set_tracing_enabled(on);
      update_status();
    });
  }
  auto filter_refresh = [this](bool) {
    refresh_from_process_trace();
    update_status();
  };
  if (show_map2d_) {
    show_map2d_->set_change(filter_refresh);
  }
  if (show_scene3d_) {
    show_scene3d_->set_change(filter_refresh);
  }
  if (show_startup_) {
    show_startup_->set_change(filter_refresh);
  }
  if (show_gdi_) {
    show_gdi_->set_change(filter_refresh);
  }
  if (show_ui_) {
    show_ui_->set_change(filter_refresh);
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({500, 260});
  add_child(std::move(loaded.root));
  set_preferred_size({500, 260});

  if (arm_ && base::trace::tracing_enabled()) {
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
  if (show_ui_) {
    show_ui_->set_change({});
  }
  remove_all_children();
  title_ = nullptr;
  status_ = nullptr;
  rollup_ = nullptr;
  toolbar_ = nullptr;
  filters_ = nullptr;
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
  show_ui_ = nullptr;
  state_.reset();
}

std::unique_ptr<RenderTracePanel> make_render_trace_panel() {
  return std::make_unique<RenderTracePanel>();
}

void RenderTracePanel::set_embedded(bool embedded) {
  embedded_ = embedded;
  // DiagnosticToolsPanel already draws Record/status. Hide the markup root so
  // FillLayout horizon cannot cover paint_self gantt (black Trace void).
  if (child_count() > 0) {
    if (View* root = child_at(0)) {
      root->set_visible(!embedded);
      root->set_preferred_size(embedded ? Size{0, 0} : Size{500, 260});
    }
  }
  const Size horizon = embedded ? Size{0, 0} : Size{480, 22};
  const Size status = embedded ? Size{0, 0} : Size{480, 20};
  const Size toolbar = embedded ? Size{0, 0} : Size{480, 32};
  const Size filters = embedded ? Size{0, 0} : Size{480, 28};
  const Size rollup = embedded ? Size{0, 0} : Size{480, 56};
  if (title_) {
    title_->set_preferred_size(horizon);
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
  if (toolbar_) {
    toolbar_->set_preferred_size(toolbar);
    toolbar_->set_visible(!embedded);
  }
  if (filters_) {
    filters_->set_preferred_size(filters);
    filters_->set_visible(!embedded);
  }
  // Keep category filters armed while horizon is hidden (embedded).
  if (embedded) {
    if (show_map2d_) {
      show_map2d_->set_checked(true);
    }
    if (show_scene3d_) {
      show_scene3d_->set_checked(true);
    }
    if (show_startup_) {
      show_startup_->set_checked(true);
    }
    if (show_gdi_) {
      show_gdi_->set_checked(true);
    }
    if (show_ui_) {
      show_ui_->set_checked(true);
    }
  }
  set_preferred_size(embedded ? Size{0, 160} : Size{500, 260});
  layout();
  schedule_paint();
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
      text += std::format("...+{} phases\n", state_->phases.size() - n);
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
  // Prefer remaining height under horizon; embedded mode collapses horizon so
  // lane_top sits just below panel top (+ inset).
  int horizon_bottom = b.y + 8;
  if (!embedded_) {
    horizon_bottom = b.y + 150;
  }
  const auto vis =
      visible_events(*state_, show_map2d_, show_scene3d_, show_startup_,
                     show_gdi_, show_ui_);
  if (vis.empty()) {
    const size_t total = state_->events.size();
    canvas->draw_text(
        b.x + 8, horizon_bottom + 4,
        utf8_to_wide(
            std::format(
                "No duration spans ({} raw events). Record UI/map work, then Refresh.",
                total))
            .c_str(),
        t.text);
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
  const int axis_h = 16;
  const int lane_bottom = b.bottom() - 6 - axis_h;
  const int lane_top = horizon_bottom;
  if (lane_bottom <= lane_top + 8 || lanes.empty()) {
    canvas->draw_text(
        b.x + 8, horizon_bottom + 4,
        utf8_to_wide(std::format("{} span(s) — expand Diagnostic Tools to plot",
                                 vis.size()))
            .c_str(),
        t.text);
    return;
  }
  const int lane_h =
      (std::max)(16, (lane_bottom - lane_top) /
                         (std::max)(1, static_cast<int>(lanes.size())));

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

  const int label_w = 96;
  const int left = b.x + label_w;
  const int right = b.right() - 8;
  const int width = (std::max)(1, right - left);
  const int plot_h = lane_bottom - lane_top;

  canvas->save();
  canvas->clip_rect(b.x + 4, lane_top, b.width - 8, plot_h + axis_h);
  canvas->fill_rect(b.x + 4, lane_top, b.width - 8, plot_h, t.control_bg);
  canvas->stroke_rect(left, lane_top, width, plot_h, t.panel_header, 1);

  // Vertical time grid (Chrome Trace style).
  for (int tick = 0; tick <= 4; ++tick) {
    const int x = left + (width * tick) / 4;
    canvas->draw_line(x, lane_top, x, lane_bottom, t.panel_header, 1);
    const double ms =
        (span * static_cast<double>(tick) / 4.0) / 1000.0;
    const std::wstring label = utf8_to_wide(std::format("{:.1f}ms", ms));
    canvas->draw_text(x + 2, lane_bottom + 2, label.c_str(), t.text_muted);
  }

  for (std::size_t i = 0; i < lanes.size(); ++i) {
    const int y = lane_top + static_cast<int>(i) * lane_h;
    if ((i % 2) == 1) {
      canvas->fill_rect(left, y, width, lane_h, t.panel_bg);
    }
    canvas->save();
    canvas->clip_rect(b.x + 4, y, label_w - 6, lane_h);
    canvas->draw_text(b.x + 6, y + std::max(0, (lane_h - 14) / 2),
                      utf8_to_wide(lanes[i]).c_str(), t.text_muted);
    canvas->restore();
    canvas->draw_line(left, y + lane_h - 1, right, y + lane_h - 1,
                      t.panel_header, 1);
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
    const int bar_h = std::max(4, lane_h - 4);
    const int x0 =
        left + static_cast<int>((static_cast<double>(ts - min_ts) / span) *
                                width);
    const int w = (std::max)(
        2, static_cast<int>((static_cast<double>((std::max)(dur, int64_t{1})) /
                             span) *
                            width));
    const ui::gfx::Color c = lane_color(li);
    canvas->fill_rect(x0, y, w, bar_h, c);
    canvas->stroke_rect(x0, y, w, bar_h, t.panel_header, 1);
    // Name when the bar is wide enough to read (DevTools / Perfetto habit).
    if (w > 48) {
      canvas->save();
      canvas->clip_rect(x0 + 2, y, w - 4, bar_h);
      canvas->draw_text(x0 + 3, y + std::max(0, (bar_h - 12) / 2),
                        utf8_to_wide(e.name).c_str(), t.text_bright);
      canvas->restore();
    }
  }
  canvas->restore();
}

}  // namespace views
}  // namespace ui
