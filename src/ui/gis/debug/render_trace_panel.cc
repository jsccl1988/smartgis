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
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {
namespace {

ui::gfx::Color lane_color(std::size_t i) {
  static const ui::gfx::Color kColors[] = {
      0xff4c8bf5u, 0xff2f9d78u, 0xffd4922au, 0xffe05a5au,
      0xff7d8590u, 0xff8e6bb0u, 0xff1aa38au, 0xffd4782au};
  return kColors[i % (sizeof(kColors) / sizeof(kColors[0]))];
}

ui::gfx::Color bar_ink(ui::gfx::Color fill) {
  const int r = static_cast<int>((fill >> 16) & 0xffu);
  const int g = static_cast<int>((fill >> 8) & 0xffu);
  const int b = static_cast<int>(fill & 0xffu);
  const int y = (r * 299 + g * 587 + b * 114) / 1000;
  return y >= 150 ? ui::gfx::color_rgb(22, 26, 32)
                  : ui::gfx::color_rgb(255, 255, 255);
}

std::string fit_label(const std::string& text, int max_px, float scale) {
  if (text.empty() || max_px <= 0) {
    return {};
  }
  if (measure_text_utf8(text, scale).width <= max_px) {
    return text;
  }
  std::string kept;
  for (unsigned char c : text) {
    std::string next = kept;
    next.push_back(static_cast<char>(c));
    if (measure_text_utf8(next + "...", scale).width > max_px) {
      break;
    }
    kept = std::move(next);
  }
  if (kept.empty()) {
    return "...";
  }
  return kept + "...";
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
  const float scale =
      widget() ? widget()->device_scale_factor() : 1.f;
  // Nominal face size underestimates Skia ink (ascent + descenders). Centering
  // and clipping on font_px shears 'y'/'g' against the row edge and bar stroke.
  const int font_px = shell_body_font_px(scale);
  const int ink_h =
      (std::max)(font_px, measure_text_utf8("Agjy", scale).height);
  const int axis_h = ink_h + 4;
  const int lane_bottom = b.bottom() - 4 - axis_h;
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
  const int lane_count = (std::max)(1, static_cast<int>(lanes.size()));
  const int view_h = lane_bottom - lane_top;
  const int natural_h = ink_h + 6;
  // Stretch only when every lane fits. Otherwise keep a readable row and scroll.
  const int fit_h = view_h > 0 ? view_h / lane_count : natural_h;
  const int lane_h = fit_h > natural_h ? fit_h : natural_h;
  const int content_h = lane_h * lane_count;
  const int max_scroll = (std::max)(0, content_h - view_h);
  if (scroll_y_ < 0) {
    scroll_y_ = 0;
  } else if (scroll_y_ > max_scroll) {
    scroll_y_ = max_scroll;
  }
  const bool scrollable = max_scroll > 0;
  constexpr int kTrackW = 12;
  const int track_w = view_h > 12 ? kTrackW : 0;

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

  int label_need = 72;
  for (const std::string& lane : lanes) {
    label_need = (std::max)(label_need, measure_text_utf8(lane, scale).width + 20);
  }
  const int label_cap = (std::max)(96, (b.width * 32) / 100);
  const int label_w = (std::min)(label_need, label_cap);
  const int left = b.x + label_w;
  const int right = b.right() - 8 - track_w;
  const int width = (std::max)(1, right - left);
  const int plot_h = lane_bottom - lane_top;
  const ui::gfx::Color grid = t.control_border;

  canvas->save();
  canvas->clip_rect(b.x + 2, lane_top, b.width - 4, plot_h + axis_h);
  canvas->fill_rect(b.x + 2, lane_top, label_w - 4, plot_h, t.panel_bg);
  canvas->fill_rect(left, lane_top, width, plot_h, t.control_bg);
  canvas->stroke_rect(left, lane_top, width, plot_h, grid, 1);

  for (int tick = 0; tick <= 4; ++tick) {
    const int x = left + (width * tick) / 4;
    canvas->draw_line(x, lane_top, x, lane_bottom, grid, 1);
    const double ms = (span * static_cast<double>(tick) / 4.0) / 1000.0;
    const std::string label_u8 =
        ms >= 1000.0 ? std::format("{:.2f}s", ms / 1000.0)
                     : std::format("{:.1f}ms", ms);
    const int label_px = measure_text_utf8(label_u8, scale).width;
    int tx = x - label_px / 2;
    if (tick == 0) {
      tx = x + 2;
    } else if (tick == 4) {
      tx = (std::min)(tx, right - label_px);
    }
    canvas->draw_text(tx, lane_bottom + 2, utf8_to_wide(label_u8).c_str(),
                      t.text);
  }

  for (std::size_t i = 0; i < lanes.size(); ++i) {
    const int y = lane_top + static_cast<int>(i) * lane_h - scroll_y_;
    if (y >= lane_bottom || y + lane_h <= lane_top) {
      continue;
    }
    const int row_h = lane_h;
    if ((i % 2) == 1) {
      canvas->fill_rect(left, y, width, row_h, t.row_alt);
      canvas->fill_rect(b.x + 2, y, label_w - 4, row_h, t.row_alt);
    }
    const int text_y = y + (std::max)(0, (row_h - ink_h) / 2);
    // Text starts at +6; leave ~10px before the lane divider, plus AA slack.
    const int label_text_w = (std::max)(8, label_w - 20);
    const std::string shown = fit_label(lanes[i], label_text_w, scale);
    canvas->save();
    canvas->clip_rect(b.x + 4, y, (std::max)(1, label_w - 14), row_h);
    canvas->draw_text(b.x + 6, text_y, utf8_to_wide(shown).c_str(), t.text);
    canvas->restore();
    canvas->draw_line(left, y + row_h - 1, right, y + row_h - 1, grid, 1);
  }
  canvas->draw_line(left, lane_top, left, lane_bottom, grid, 1);

  for (const auto& e : vis) {
    const auto ts = std::chrono::duration_cast<base::trace::Trace::duration>(
                        e.begin - state_->origin)
                        .count();
    const auto dur =
        std::chrono::duration_cast<base::trace::Trace::duration>(e.end - e.begin)
            .count();
    const std::size_t li = lane_of[lane_key(e)];
    const int row_y = lane_top + static_cast<int>(li) * lane_h - scroll_y_;
    if (row_y >= lane_bottom || row_y + lane_h <= lane_top) {
      continue;
    }
    const int row_h = lane_h;
    // Keep the bar a fraction of the row. Pull the inset in only when the
    // remaining height cannot hold full ink above the stroke.
    int inset = (std::max)(2, row_h / 6);
    if (row_h - inset * 2 < ink_h) {
      inset = (std::max)(0, (row_h - ink_h) / 2);
    }
    const int y = row_y + inset;
    const int bar_h = (std::max)(ink_h, row_h - inset * 2);
    const int x0 =
        left + static_cast<int>((static_cast<double>(ts - min_ts) / span) *
                                width);
    int w = (std::max)(
        3, static_cast<int>((static_cast<double>((std::max)(dur, int64_t{1})) /
                             span) *
                            width));
    if (x0 + w > right) {
      w = (std::max)(3, right - x0);
    }
    const ui::gfx::Color c = lane_color(li);
    canvas->fill_rect(x0, y, w, bar_h, c);
    canvas->stroke_rect(x0, y, w, bar_h, grid, 1);
    const int name_pad = 6;
    if (w > name_pad * 2 + 8 && !e.name.empty()) {
      const std::string name =
          fit_label(e.name, w - name_pad * 2, scale);
      const int name_y = y + (std::max)(0, (bar_h - ink_h) / 2);
      canvas->save();
      // Inset past the stroke so glyphs stay inside the filled bar.
      canvas->clip_rect(x0 + 2, y + 1, (std::max)(1, w - 4),
                        (std::max)(1, bar_h - 2));
      canvas->draw_text(x0 + name_pad, name_y, utf8_to_wide(name).c_str(),
                        bar_ink(c));
      canvas->restore();
    }
  }
  canvas->restore();

  plot_content_h_ = content_h;
  plot_view_h_ = view_h;
  plot_lane_h_ = lane_h;
  plot_scrollable_ = scrollable && track_w > 0;
  if (track_w <= 0) {
    plot_track_ = {};
    plot_thumb_ = {};
    return;
  }
  const int track_h = view_h + axis_h;
  plot_track_ = {b.right() - 8 - kTrackW, lane_top, kTrackW, track_h};
  int thumb_h = scrollable ? track_h * view_h / (std::max)(1, content_h) : track_h;
  if (thumb_h < 12) {
    thumb_h = 12;
  }
  if (thumb_h > track_h) {
    thumb_h = track_h;
  }
  const int travel = (std::max)(0, track_h - thumb_h);
  const int thumb_y =
      max_scroll > 0 ? plot_track_.y + travel * scroll_y_ / max_scroll
                     : plot_track_.y;
  plot_thumb_ = {plot_track_.x, thumb_y, plot_track_.width, thumb_h};
  canvas->fill_rect(plot_track_.x, plot_track_.y, plot_track_.width,
                    plot_track_.height, t.panel_header);
  canvas->fill_rect(plot_thumb_.x + 2, plot_thumb_.y,
                    (std::max)(1, plot_thumb_.width - 4), plot_thumb_.height,
                    scrollable ? t.control_hover : t.control_border);
}

bool RenderTracePanel::on_mouse_event(const MouseEvent& event) {
  if (!is_enabled()) {
    return false;
  }
  const int max_scroll = (std::max)(0, plot_content_h_ - plot_view_h_);
  auto apply_scroll = [this](int next) {
    const int capped = std::clamp(
        next, 0, (std::max)(0, plot_content_h_ - plot_view_h_));
    if (capped == scroll_y_) {
      return;
    }
    scroll_y_ = capped;
    schedule_paint();
  };
  auto seek_from_y = [&](int y) {
    if (!plot_scrollable_ || plot_track_.height <= 0) {
      return;
    }
    int thumb_h =
        plot_track_.height * plot_view_h_ / (std::max)(1, plot_content_h_);
    if (thumb_h < 12) {
      thumb_h = 12;
    }
    if (thumb_h > plot_track_.height) {
      thumb_h = plot_track_.height;
    }
    const int travel = (std::max)(1, plot_track_.height - thumb_h);
    const int rel = std::clamp(y - plot_track_.y - thumb_h / 2, 0, travel);
    apply_scroll(max_scroll > 0 ? rel * max_scroll / travel : 0);
  };

  if (scroll_drag_ && (event.type == MouseEvent::Type::kMove ||
                       event.type == MouseEvent::Type::kUp)) {
    if (event.type == MouseEvent::Type::kUp) {
      scroll_drag_ = false;
    }
    seek_from_y(event.y);
    return true;
  }
  if (!bounds().contains(event.x, event.y)) {
    return false;
  }
  if (event.type == MouseEvent::Type::kWheel) {
    if (!plot_scrollable_) {
      return false;
    }
    int steps = event.wheel_delta / 120;
    if (steps == 0 && event.wheel_delta != 0) {
      steps = event.wheel_delta > 0 ? 1 : -1;
    }
    const int step = (std::max)(plot_lane_h_, 24);
    apply_scroll(scroll_y_ - steps * step);
    return true;
  }
  if (event.type == MouseEvent::Type::kDown && event.button == 1 &&
      plot_scrollable_ && plot_track_.contains(event.x, event.y)) {
    scroll_drag_ = true;
    seek_from_y(event.y);
    return true;
  }
  return false;
}

}  // namespace views
}  // namespace ui
