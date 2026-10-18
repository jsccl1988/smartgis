// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/debug/diagnostic_tools_panel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <dbghelp.h>

#include "base/log/agent_feed.h"
#include "base/memory/allocation_tracker.h"
#include "base/memory/arena.h"
#include "base/memory/sample_trace.h"
#include "base/trace/diag/diagnostic_bootstrap.h"
#include "base/trace/event/process_trace.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/debug/render_trace_panel.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/shell/dpi.h"
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

// Bottom dock must clear Diagnostic chrome (title 22 + status 20 + toolbar 26),
// the tab header (26), and the Output toolbar (24) and still show several log
// rows. 140 DIP left about one clipped line (shell capture diag_dock_before).
// SecondaryFixed still caps the dock at ~1/3 of the split so the map stays
// primary when a short HiDPI client cannot honor this preferred height.
constexpr int kDiagPreferredDip = 260;
constexpr int kTabsHostMinDip = 110;
constexpr int kToolbarHeightDip = 26;
constexpr int kTitleHeightDip = 22;
constexpr int kStatusHeightDip = 20;

std::string agent_session_key(const base::AgentFeedEntry& row) {
  return row.session.empty() ? std::string("console") : row.session;
}

void paint_agent_rows(ui::gfx::Canvas* canvas, const Rect& b, float scale,
                      const std::vector<base::AgentFeedEntry>& rows) {
  const Theme& t = Theme::current();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
  const int font_px = shell_body_font_px(scale);
  const int rh = font_px + 8;
  if (rows.empty()) {
    canvas->draw_text(b.x + 8, b.y + 6, L"Waiting for this session.",
                      t.text_muted);
    return;
  }
  const int capacity = (std::max)(1, b.height / (std::max)(1, rh));
  const int start = (std::max)(0, static_cast<int>(rows.size()) - capacity);
  const int kind_w = measure_text_utf8("DENY", scale).width + 12;
  const int time_w = measure_text_utf8("00:00:00", scale).width + 10;
  for (int i = start; i < static_cast<int>(rows.size()); ++i) {
    const base::AgentFeedEntry& row = rows[static_cast<std::size_t>(i)];
    const int y = b.y + (i - start) * rh;
    if ((i % 2) == 1) {
      canvas->fill_rect(b.x, y, b.width, rh, t.panel_bg);
    }
    const int text_y = y + (std::max)(0, (rh - font_px) / 2);
    std::string clock = row.timestamp;
    if (clock.size() >= 8) {
      clock = clock.substr(clock.size() - 8);
    }
    canvas->draw_text(b.x + 6, text_y, utf8_to_wide(clock).c_str(),
                      t.text_muted);
    const wchar_t* tag = L"CMD";
    ui::gfx::Color tag_color = t.accent;
    switch (row.kind) {
      case base::AgentFeedKind::kResult:
        tag = L"OUT";
        tag_color = t.text;
        break;
      case base::AgentFeedKind::kRpc:
        tag = L"RPC";
        tag_color = ui::gfx::color_rgb(140, 180, 230);
        break;
      case base::AgentFeedKind::kDeny:
        tag = L"DENY";
        tag_color = t.danger;
        break;
      case base::AgentFeedKind::kCommand:
        break;
    }
    canvas->draw_text(b.x + 6 + time_w, text_y, tag, tag_color);
    const int msg_x = b.x + 6 + time_w + kind_w;
    const int msg_w = (std::max)(0, b.right() - 6 - msg_x);
    if (msg_w > 0) {
      canvas->save();
      canvas->clip_rect(msg_x, y, msg_w, rh);
      canvas->draw_text(msg_x, text_y, utf8_to_wide(row.text).c_str(), t.text);
      canvas->restore();
    }
  }
}

// One session's command / result list. The ring lives in base.dll.
class AgentSessionPage : public View {
 public:
  explicit AgentSessionPage(std::string session) : session_(std::move(session)) {}

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const float scale = widget() ? widget()->device_scale_factor() : 1.f;
    std::vector<base::AgentFeedEntry> mine;
    for (const base::AgentFeedEntry& row : base::agent_feed().snapshot()) {
      if (agent_session_key(row) == session_) {
        mine.push_back(row);
      }
    }
    paint_agent_rows(canvas, bounds(), scale, mine);
  }

 private:
  std::string session_;
};

// Agent tab body. Each DebugAgent connection is a bottom sub-tab.
class AgentSessionsPage : public View {
 public:
  AgentSessionsPage() {
    auto strip = std::make_unique<TabStrip>();
    strip->set_header_placement(TabStrip::HeaderPlacement::kBottom);
    strip_ = strip.get();
    strip_->set_change([this](int) { refresh_active(); });
    set_layout_manager(std::make_unique<FillLayout>());
    add_child(std::move(strip));
    sub_id_ = base::agent_feed().subscribe([this](const base::AgentFeedEntry&) {
      arm_flush();
    });
  }

  ~AgentSessionsPage() override {
    alive_.store(false);
    disarm_flush();
    if (sub_id_ != 0) {
      base::agent_feed().unsubscribe(sub_id_);
      sub_id_ = 0;
    }
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    arm_flush();
    if (strip_ && strip_->tab_count() > 0) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
    canvas->draw_text(b.x + 8, b.y + 6, L"Waiting for DebugAgent commands.",
                      t.text_muted);
  }

 private:
  void sync_sessions() {
    if (!strip_ || sessions_.size() >= 32) {
      return;
    }
    bool added = false;
    for (const base::AgentFeedEntry& row : base::agent_feed().snapshot()) {
      if (sessions_.size() >= 32) {
        break;
      }
      const std::string key = agent_session_key(row);
      bool known = false;
      for (const std::string& have : sessions_) {
        if (have == key) {
          known = true;
          break;
        }
      }
      if (known) {
        continue;
      }
      sessions_.push_back(key);
      strip_->add_tab(key, std::make_unique<AgentSessionPage>(key));
      added = true;
    }
    if (!added) {
      return;
    }
    strip_->layout();
    strip_->invalidate_commands();
    strip_->schedule_paint();
    invalidate_commands();
    schedule_paint();
    refresh_active();
  }

  void refresh_active() {
    if (!strip_ || strip_->tab_count() <= 0) {
      invalidate_commands();
      schedule_paint();
      return;
    }
    if (View* page = strip_->page_at(strip_->active())) {
      page->invalidate_commands();
      page->schedule_paint();
    }
  }

  void arm_flush() {
    if (!alive_.load()) {
      return;
    }
    HWND hwnd = widget() ? widget()->hwnd() : nullptr;
    if (!hwnd) {
      return;
    }
    if (flush_armed_.exchange(true)) {
      return;
    }
    if (!SetTimer(hwnd, reinterpret_cast<UINT_PTR>(this), 50,
                  &AgentSessionsPage::on_flush_timer)) {
      flush_armed_.store(false);
    }
  }

  void disarm_flush() {
    flush_armed_.store(false);
    HWND hwnd = widget() ? widget()->hwnd() : nullptr;
    if (hwnd) {
      KillTimer(hwnd, reinterpret_cast<UINT_PTR>(this));
    }
  }

  static void CALLBACK on_flush_timer(HWND hwnd, UINT, UINT_PTR id, DWORD) {
    KillTimer(hwnd, id);
    auto* self = reinterpret_cast<AgentSessionsPage*>(id);
    if (!self || !self->alive_.load()) {
      return;
    }
    self->flush_armed_.store(false);
    self->sync_sessions();
    self->refresh_active();
  }

  TabStrip* strip_ = nullptr;
  std::vector<std::string> sessions_;
  std::uint64_t sub_id_ = 0;
  std::atomic<bool> alive_{true};
  std::atomic<bool> flush_armed_{false};
};

std::string memory_frame_label(void* pc) {
  static std::unordered_map<void*, std::string> cache;
  if (!pc) {
    return {};
  }
  if (const auto it = cache.find(pc); it != cache.end()) {
    return it->second;
  }

  std::string resolved;
  using SymSetOptionsFn = DWORD(WINAPI*)(DWORD);
  using SymInitializeFn = BOOL(WINAPI*)(HANDLE, PCSTR, BOOL);
  using SymFromAddrFn = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
  static HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
  static SymSetOptionsFn set_opts = dbghelp
      ? reinterpret_cast<SymSetOptionsFn>(GetProcAddress(dbghelp, "SymSetOptions"))
      : nullptr;
  static SymInitializeFn init = dbghelp
      ? reinterpret_cast<SymInitializeFn>(GetProcAddress(dbghelp, "SymInitialize"))
      : nullptr;
  static SymFromAddrFn from_addr = dbghelp
      ? reinterpret_cast<SymFromAddrFn>(GetProcAddress(dbghelp, "SymFromAddr"))
      : nullptr;
  static bool attempted = false;
  static bool ready = false;
  if (!attempted && set_opts && init && from_addr) {
    attempted = true;
    set_opts(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    ready = init(GetCurrentProcess(), nullptr, TRUE) == TRUE;
  }
  if (ready && from_addr) {
    alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256];
    auto* info = reinterpret_cast<SYMBOL_INFO*>(buf);
    info->SizeOfStruct = sizeof(SYMBOL_INFO);
    info->MaxNameLen = 255;
    DWORD64 disp = 0;
    if (from_addr(GetCurrentProcess(), reinterpret_cast<DWORD64>(pc), &disp,
                  info) &&
        info->Name[0] != '\0') {
      resolved = info->Name;
    }
  }
  if (resolved.empty()) {
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(pc), &mod) &&
        mod) {
      char path[MAX_PATH] = {};
      GetModuleFileNameA(mod, path, MAX_PATH);
      std::string file(path);
      const auto slash = file.find_last_of("\\/");
      if (slash != std::string::npos) {
        file.erase(0, slash + 1);
      }
      const auto base = reinterpret_cast<uintptr_t>(mod);
      const auto addr = reinterpret_cast<uintptr_t>(pc);
      resolved = std::format(
          "{}+0x{:x}", file,
          static_cast<unsigned long long>(addr - base));
    } else {
      resolved = std::format("0x{:x}",
                             static_cast<unsigned long long>(
                                 reinterpret_cast<uintptr_t>(pc)));
    }
  }
  cache.emplace(pc, resolved);
  return resolved;
}

bool skip_tracker_frame(const std::string& name) {
  return name.find("AllocationTracker") != std::string::npos ||
         name.find("ObjectAllocator") != std::string::npos ||
         name.find("capture_alloc_stack") != std::string::npos;
}

struct FlameBar {
  std::string label;
  uint64_t bytes = 0;
  uint32_t count = 0;
  std::vector<FlameBar> children;
};

void append_flame_children(FlameBar& parent, const base::AllocationTracker::FlameNode& node);

FlameBar flame_bar_from(const base::AllocationTracker::FlameNode& node, bool root) {
  FlameBar bar;
  bar.bytes = node.bytes;
  bar.count = node.count;
  if (root) {
    bar.label = "live";
  } else if (node.type && node.type[0] != '\0') {
    bar.label = node.type;
  } else {
    bar.label = memory_frame_label(node.pc);
  }
  append_flame_children(bar, node);
  std::sort(bar.children.begin(), bar.children.end(),
            [](const FlameBar& a, const FlameBar& b) { return a.bytes > b.bytes; });
  return bar;
}

void append_flame_children(FlameBar& parent,
                           const base::AllocationTracker::FlameNode& node) {
  for (const base::AllocationTracker::FlameNode& child : node.children) {
    const bool typed = child.type && child.type[0] != '\0';
    const std::string label = typed ? std::string(child.type) : memory_frame_label(child.pc);
    if (!typed && skip_tracker_frame(label)) {
      append_flame_children(parent, child);
      continue;
    }
    parent.children.push_back(flame_bar_from(child, false));
  }
}

std::string flame_bytes(uint64_t n) {
  constexpr uint64_t kKiB = 1024;
  constexpr uint64_t kMiB = 1024 * kKiB;
  if (n >= kMiB) {
    return std::format("{:.1f} MiB", static_cast<double>(n) / static_cast<double>(kMiB));
  }
  if (n >= kKiB) {
    return std::format("{:.1f} KiB", static_cast<double>(n) / static_cast<double>(kKiB));
  }
  return std::format("{} B", n);
}

ui::gfx::Color flame_color(const std::string& label) {
  static const ui::gfx::Color kPalette[] = {
      ui::gfx::color_rgb(224, 108, 69),  ui::gfx::color_rgb(212, 160, 23),
      ui::gfx::color_rgb(194, 59, 34),   ui::gfx::color_rgb(239, 139, 44),
      ui::gfx::color_rgb(184, 92, 56),   ui::gfx::color_rgb(240, 193, 74),
      ui::gfx::color_rgb(163, 59, 36),   ui::gfx::color_rgb(227, 139, 74),
  };
  uint32_t h = 2166136261u;
  for (unsigned char c : label) {
    h ^= c;
    h *= 16777619u;
  }
  return kPalette[h % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

int flame_depth(const FlameBar& bar) {
  int depth = 1;
  for (const FlameBar& child : bar.children) {
    depth = (std::max)(depth, 1 + flame_depth(child));
  }
  return depth;
}

void paint_flame_bar(ui::gfx::Canvas* canvas, const FlameBar& bar, int x, int y,
                     int w, int row_h, int top, int bottom) {
  if (!canvas || w < 1 || bar.bytes == 0) {
    return;
  }
  if (y < bottom && y + row_h > top) {
    const bool root = bar.label == "live";
    const ui::gfx::Color fill =
        root ? ui::gfx::color_rgb(44, 62, 80) : flame_color(bar.label);
    canvas->fill_rect(x, y, w, row_h - 1, fill);
    if (w >= 24) {
      const std::string text =
          bar.count > 1 ? std::format("{} {} x{}", bar.label,
                                      flame_bytes(bar.bytes), bar.count)
                        : std::format("{} {}", bar.label, flame_bytes(bar.bytes));
      canvas->save();
      canvas->clip_rect(x + 2, y, w - 4, row_h - 1);
      canvas->draw_text(x + 3, y + 1, utf8_to_wide(text).c_str(),
                        ui::gfx::color_rgb(255, 247, 240));
      canvas->restore();
    }
  }
  // Children are stacked above this row. Past the top, they stay clipped.
  if (y < top) {
    return;
  }
  const int child_y = y - row_h;
  int cx = x;
  for (const FlameBar& child : bar.children) {
    if (child.bytes == 0 || cx >= x + w) {
      continue;
    }
    int cw = static_cast<int>(
        (static_cast<double>(child.bytes) / static_cast<double>(bar.bytes)) * w);
    if (cw < 1) {
      cw = 1;
    }
    if (cx + cw > x + w) {
      cw = x + w - cx;
    }
    paint_flame_bar(canvas, child, cx, child_y, cw, row_h, top, bottom);
    cx += cw;
  }
}

// Working-set area chart. Samples are pushed on the UI thread every 500ms and
// do not depend on span tracing being armed (Stop leaves this chart running).
class MemoryChartView : public View {
 public:
  struct Sample {
    int64_t working_set = 0;
    int64_t tls_used = 0;
    int64_t tracker_live = 0;
  };

  void push(Sample sample, bool schedule = true) {
    samples_.push_back(sample);
    constexpr size_t kMaxSamples = 120;
    if (samples_.size() > kMaxSamples) {
      samples_.erase(samples_.begin(),
                     samples_.begin() +
                         static_cast<std::ptrdiff_t>(samples_.size() -
                                                     kMaxSamples));
    }
    if (schedule) {
      schedule_paint();
    }
  }

  void clear_samples(bool schedule = true) {
    samples_.clear();
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
    if (samples_.empty()) {
      canvas->draw_text(b.x + 8, b.y + 8,
                        L"Waiting for memory samples (always-on 500ms)",
                        t.text_muted);
      return;
    }

    int64_t max_v = 1;
    for (const Sample& s : samples_) {
      max_v = (std::max)(max_v, s.working_set);
    }
    const int left = b.x + 8;
    const int right = b.right() - 8;
    const int top = b.y + 8;
    const int bottom = b.bottom() - 8;
    const int width = (std::max)(1, right - left);
    const int height = (std::max)(1, bottom - top);
    canvas->fill_rect(left, top, width, height, t.control_bg);

    for (int g = 1; g <= 3; ++g) {
      const int gy = bottom - (height * g) / 4;
      canvas->fill_rect(left, gy, width, 1, t.control_border);
    }

    const int n = static_cast<int>(samples_.size());
    const ui::gfx::Color area = ui::gfx::color_rgb(28, 96, 168);
    const ui::gfx::Color edge = ui::gfx::color_rgb(126, 182, 255);
    int prev_x = left;
    int prev_y = bottom;
    for (int i = 0; i < n; ++i) {
      const int x0 = left + (i * width) / n;
      const int x1 = left + ((i + 1) * width) / n;
      const int col_w = (std::max)(1, x1 - x0);
      const int col_h = static_cast<int>(
          (static_cast<double>(samples_[static_cast<size_t>(i)].working_set) /
           static_cast<double>(max_v)) *
          height);
      const int y = bottom - col_h;
      canvas->fill_rect(x0, y, col_w, (std::max)(1, col_h), area);
      const int mid_x = x0 + col_w / 2;
      if (i > 0) {
        canvas->draw_line(prev_x, prev_y, mid_x, y, edge, 2);
      }
      prev_x = mid_x;
      prev_y = y;
    }
  }

 private:
  std::vector<Sample> samples_;
};

// Per-object flame graph (pprof style): width is live bytes, leaf is the type.
class MemoryFlameView : public View {
 public:
  void set_tree(std::vector<base::AllocationTracker::FlameNode> nodes,
                bool tracking, bool schedule = true) {
    bars_.clear();
    tracking_ = tracking;
    if (!nodes.empty()) {
      bars_.push_back(flame_bar_from(nodes.front(), true));
    }
    const int content_h =
        bars_.empty() ? 0 : flame_depth(bars_.front()) * kRow;
    const int max_scroll = (std::max)(0, content_h - last_view_h_);
    if (scroll_y_ > max_scroll) {
      scroll_y_ = max_scroll;
    }
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
    const int left = b.x + 8;
    const int top = b.y + 4;
    const int right = b.right() - 8;
    const int bottom = b.bottom() - 4;
    const int width = (std::max)(1, right - left);
    const int height = (std::max)(1, bottom - top);
    canvas->fill_rect(left, top, width, height, t.control_bg);
    if (bars_.empty()) {
      scrollable_ = false;
      const wchar_t* msg =
          tracking_ ? L"Track allocs is on — new base::create objects show here"
                    : L"Turn on Track allocs for a per-object flame graph";
      canvas->draw_text(left + 6, top + 6, msg, t.text_muted);
      return;
    }
    const int content_h = flame_depth(bars_.front()) * kRow;
    const int max_scroll = (std::max)(0, content_h - height);
    if (scroll_y_ > max_scroll) {
      scroll_y_ = max_scroll;
    }
    if (scroll_y_ < 0) {
      scroll_y_ = 0;
    }
    last_view_h_ = height;
    const bool scrollable = max_scroll > 0;
    constexpr int kTrackW = 10;
    const int track_w = scrollable ? kTrackW : 0;
    const int plot_w = (std::max)(1, width - track_w);
    canvas->save();
    canvas->clip_rect(left, top, plot_w, height);
    paint_flame_bar(canvas, bars_.front(), left, bottom - kRow + scroll_y_,
                    plot_w, kRow, top, bottom);
    canvas->restore();

    content_h_ = content_h;
    view_h_ = height;
    scrollable_ = scrollable;
    if (!scrollable) {
      track_ = {};
      thumb_ = {};
      return;
    }
    track_ = {right - kTrackW, top, kTrackW, height};
    int thumb_h = height * height / (std::max)(1, content_h);
    if (thumb_h < 12) {
      thumb_h = 12;
    }
    if (thumb_h > height) {
      thumb_h = height;
    }
    const int travel = (std::max)(0, height - thumb_h);
    const int thumb_y =
        max_scroll > 0 ? track_.y + travel * scroll_y_ / max_scroll : track_.y;
    thumb_ = {track_.x, thumb_y, track_.width, thumb_h};
    canvas->fill_rect(track_.x, track_.y, track_.width, track_.height,
                      t.panel_header);
    canvas->fill_rect(thumb_.x + 1, thumb_.y, (std::max)(1, thumb_.width - 2),
                      thumb_.height, t.control_hover);
  }

  bool on_mouse_event(const MouseEvent& event) override {
    if (!is_enabled()) {
      return false;
    }
    const int max_scroll = (std::max)(0, content_h_ - view_h_);
    auto apply_scroll = [this](int next) {
      const int capped =
          std::clamp(next, 0, (std::max)(0, content_h_ - view_h_));
      if (capped == scroll_y_) {
        return;
      }
      scroll_y_ = capped;
      schedule_paint();
    };
    auto seek_from_y = [&](int y) {
      if (!scrollable_ || track_.height <= 0) {
        return;
      }
      int thumb_h = track_.height * view_h_ / (std::max)(1, content_h_);
      if (thumb_h < 12) {
        thumb_h = 12;
      }
      if (thumb_h > track_.height) {
        thumb_h = track_.height;
      }
      const int travel = (std::max)(1, track_.height - thumb_h);
      const int rel = std::clamp(y - track_.y - thumb_h / 2, 0, travel);
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
      if (!scrollable_) {
        return false;
      }
      int steps = event.wheel_delta / 120;
      if (steps == 0 && event.wheel_delta != 0) {
        steps = event.wheel_delta > 0 ? 1 : -1;
      }
      apply_scroll(scroll_y_ - steps * kRow);
      return true;
    }
    if (event.type == MouseEvent::Type::kDown && event.button == 1 &&
        scrollable_ && track_.contains(event.x, event.y)) {
      scroll_drag_ = true;
      seek_from_y(event.y);
      return true;
    }
    return false;
  }

 private:
  static constexpr int kRow = 18;

  std::vector<FlameBar> bars_;
  bool tracking_ = false;
  int scroll_y_ = 0;
  int content_h_ = 0;
  int view_h_ = 0;
  int last_view_h_ = 0;
  bool scrollable_ = false;
  bool scroll_drag_ = false;
  Rect track_{};
  Rect thumb_{};
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
    View* flame_host = loaded.ids.find("flame_host");

    auto chart = std::make_unique<MemoryChartView>();
    chart_ = chart.get();
    if (chart_host) {
      chart_host->set_layout_manager(std::make_unique<FillLayout>());
      chart_host->add_child(std::move(chart));
    }
    auto flame = std::make_unique<MemoryFlameView>();
    flame_ = flame.get();
    if (flame_host) {
      flame_host->set_layout_manager(std::make_unique<FillLayout>());
      flame_host->add_child(std::move(flame));
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({0, 220});
    add_child(std::move(loaded.root));
    set_preferred_size({0, 220});
  }

  Label* stats_label() { return stats_; }

  void push_sample(int64_t working_set, int64_t tls_used, int64_t tracker_live,
                   bool schedule = true) {
    if (chart_) {
      chart_->push(MemoryChartView::Sample{working_set, tls_used, tracker_live},
                   schedule);
    }
  }

  void clear_samples(bool schedule = true) {
    if (chart_) {
      chart_->clear_samples(schedule);
    }
  }

  void set_flame(std::vector<base::AllocationTracker::FlameNode> nodes,
                 bool tracking, bool schedule = true) {
    if (flame_) {
      flame_->set_tree(std::move(nodes), tracking, schedule);
    }
  }

 private:
  Label* stats_ = nullptr;
  MemoryChartView* chart_ = nullptr;
  MemoryFlameView* flame_ = nullptr;
};

}  // namespace

DiagnosticToolsPanel::DiagnosticToolsPanel() {
  MarkupRoot loaded = load_markup("debug/diagnostic_tools_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({0, 0});
    return;
  }
  panel_root_ = loaded.root.get();
  title_ = loaded.ids.find_as<Label>("title");
  status_ = loaded.ids.find_as<Label>("status");
  toolbar_ = loaded.ids.find("toolbar");
  record_ = loaded.ids.find_as<Button>("record");
  stop_ = loaded.ids.find_as<Button>("stop");
  clear_ = loaded.ids.find_as<Button>("clear");
  export_ = loaded.ids.find_as<Button>("export");
  refresh_ = loaded.ids.find_as<Button>("refresh");
  arm_ = loaded.ids.find_as<Checkbox>("arm");
  track_allocs_ = loaded.ids.find_as<Checkbox>("track_allocs");
  echo_commands_ = loaded.ids.find_as<Checkbox>("echo_commands");
  tabs_host_ = loaded.ids.find("tabs_host");

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

  // Cold-start: Output stays on LogSink for the process lifetime and is the
  // landing tab. Console + Memory markup trees wait until first select.
  auto output = std::make_unique<DebugConsolePanel>();
  output->set_pane_mode(DebugConsolePanel::PaneMode::kOutput);
  output->set_visible_console(true);
  output_ = output.get();

  auto cpu = std::make_unique<RenderTracePanel>();
  cpu->set_embedded(true);
  cpu_ = cpu.get();

  auto tabs = std::make_unique<TabStrip>();
  tabs->set_preferred_size({0, kTabsHostMinDip});
  tabs_ = tabs.get();
  tabs_->add_tab("Output", std::move(output));
  console_tab_ = tabs_->add_tab("Console", std::make_unique<View>());
  tabs_->add_tab("Trace", std::move(cpu));
  memory_tab_ = tabs_->add_tab("Memory", std::make_unique<View>());
  tabs_->add_tab("Agent", std::make_unique<AgentSessionsPage>());
  tabs_->set_change([this](int i) { ensure_tab_content(i); });
  tabs_->set_active(0);

  if (tabs_host_) {
    tabs_host_->set_preferred_size({0, kTabsHostMinDip});
    tabs_host_->set_layout_manager(std::make_unique<FillLayout>());
    tabs_host_->add_child(std::move(tabs));
  }

  // Keep markup Yoga (.tabs_host { flex-grow:1 }) so Trace/Console body
  // retains height. Prefer C++ preferred sizes over CSS px heights.
  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({0, kDiagPreferredDip});
  add_child(std::move(loaded.root));
  set_preferred_size({0, 0});
  apply_frame_metrics(1.f);

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
  // Start collapsed: hide horizon children so a thin splitter remnant cannot
  // paint "Diagnostic Tools" / tabs into the status-bar band.
  for (size_t i = 0; i < child_count(); ++i) {
    if (View* c = child_at(i)) {
      c->set_visible(false);
    }
  }
}

DiagnosticToolsPanel::~DiagnosticToolsPanel() {
  alive_.store(false);
  disarm_memory_timer();
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
  panel_root_ = nullptr;
  toolbar_ = nullptr;
  tabs_host_ = nullptr;
  panel_box_ = nullptr;
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
  const float scale =
      widget() ? widget()->device_scale_factor() : 1.f;
  const int diag_h = on ? dip_to_px(kDiagPreferredDip, scale) : 0;
  set_preferred_size({0, diag_h});
  // FillLayout host is the Splitter pane — pin it too so seed uses stored
  // secondary (not an inflated Console get_preferred_size).
  if (View* host = parent()) {
    host->set_preferred_size({0, diag_h});
  }
  // Hide horizon while collapsed so children cannot paint into a remnant strip.
  // Output keeps LogSink subscription even while collapsed so RHI / present
  // LOGGING still accumulates and snapshot_tail is not the only recovery path.
  for (size_t i = 0; i < child_count(); ++i) {
    if (View* c = child_at(i)) {
      c->set_visible(on);
    }
  }
  if (output_) {
    output_->set_visible_console(true);
    output_->sync_log_output();
  }
  if (console_) {
    console_->set_visible_console(on);
  }
  if (on) {
    apply_frame_metrics(scale);
    on_refresh();
    last_auto_refresh_ = std::chrono::steady_clock::now();
    arm_memory_timer();
  } else {
    disarm_memory_timer();
  }
  reseed_host_splitter();
  schedule_paint();
}

void DiagnosticToolsPanel::apply_frame_metrics(float scale) {
  if (scale <= 0.f) {
    scale = 1.f;
  }
  auto set_h = [&](View* v, int h_dip) {
    if (!v) {
      return;
    }
    const Size cur = v->preferred_size();
    v->set_preferred_size({cur.width, dip_to_px(h_dip, scale)});
  };
  set_h(title_, kTitleHeightDip);
  set_h(status_, kStatusHeightDip);
  set_h(toolbar_, kToolbarHeightDip);
  if (tabs_host_) {
    tabs_host_->set_preferred_size({0, dip_to_px(kTabsHostMinDip, scale)});
  }
  if (tabs_) {
    tabs_->set_preferred_size({0, dip_to_px(kTabsHostMinDip, scale)});
  }
  if (panel_root_) {
    panel_root_->set_preferred_size({0, dip_to_px(kDiagPreferredDip, scale)});
  }
  if (toolbar_) {
    if (auto* box = dynamic_cast<BoxLayout*>(toolbar_->layout_manager())) {
      box->set_between_child_spacing(dip_to_px(6, scale));
    } else {
      auto row =
          std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
      row->set_between_child_spacing(dip_to_px(6, scale));
      toolbar_->set_layout_manager(std::move(row));
    }
  }
  if (visible_) {
    set_preferred_size({0, dip_to_px(kDiagPreferredDip, scale)});
  }
}

void DiagnosticToolsPanel::reseed_host_splitter() {
  // Parent may be diagnostic_host (FillLayout) under main_split — walk up.
  for (View* p = parent(); p; p = p->parent()) {
    if (auto* split = dynamic_cast<Splitter*>(p)) {
      split->reseed(/*force=*/true);
      return;
    }
  }
  if (View* p = parent()) {
    p->layout();
  } else {
    layout();
  }
}

void DiagnosticToolsPanel::ensure_console_tab() {
  if (console_ || !tabs_ || console_tab_ < 0) {
    return;
  }
  auto console = std::make_unique<DebugConsolePanel>();
  console->set_pane_mode(DebugConsolePanel::PaneMode::kConsole);
  console->set_visible_console(visible_);
  console_ = console.get();
  console_->set_echo_to_output([this](const std::string& line) {
    if (echo_commands_ && echo_commands_->is_checked() && output_) {
      output_->append_line(line);
    }
  });
  if (pending_console_submit_) {
    console_->set_submit_handler(pending_console_submit_);
  }
  tabs_->replace_page(console_tab_, std::move(console));
}

void DiagnosticToolsPanel::ensure_memory_tab() {
  if (memory_page_ || !tabs_ || memory_tab_ < 0) {
    return;
  }
  auto memory = std::make_unique<MemoryPageView>();
  memory_stats_ = memory->stats_label();
  memory_page_ = memory.get();
  tabs_->replace_page(memory_tab_, std::move(memory));
  push_memory_sample(/*schedule=*/true);
  refresh_memory_stats();
}

void DiagnosticToolsPanel::ensure_tab_content(int index) {
  if (index == console_tab_) {
    ensure_console_tab();
  } else if (index == memory_tab_) {
    ensure_memory_tab();
  }
}

void DiagnosticToolsPanel::set_active_tab(int index) {
  if (!tabs_ || index < 0 || index >= tabs_->tab_count()) {
    return;
  }
  ensure_tab_content(index);
  tabs_->set_active(index);
}

int DiagnosticToolsPanel::active_tab() const {
  return tabs_ ? tabs_->active() : -1;
}

void DiagnosticToolsPanel::set_console_submit(
    std::function<void(const std::string&)> fn) {
  pending_console_submit_ = std::move(fn);
  if (console_) {
    console_->set_submit_handler(pending_console_submit_);
  }
}

void DiagnosticToolsPanel::on_device_scale_factor_changed(float old_scale,
                                                         float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  apply_frame_metrics(new_scale);
}

void DiagnosticToolsPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas || !visible_) {
    return;
  }
  // Do NOT schedule_paint here — that caused full-rate shell republish flicker.
  // Memory samples ride a 500ms timer; arm it once the HWND exists.
  arm_memory_timer();
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
    mem->clear_samples();
    mem->set_flame({}, base::AllocationTracker::is_enabled());
  }
  base::AllocationTracker::reset();
  base::agent_feed().clear();
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
  push_memory_sample(/*schedule=*/true);
  refresh_memory_stats();
  update_status();
}

namespace {

std::string format_mem_bytes(uint64_t n) {
  constexpr uint64_t kKiB = 1024;
  constexpr uint64_t kMiB = 1024 * kKiB;
  constexpr uint64_t kGiB = 1024 * kMiB;
  if (n >= kGiB) {
    return std::format("{:.1f} GiB",
                       static_cast<double>(n) / static_cast<double>(kGiB));
  }
  if (n >= kMiB) {
    return std::format("{:.1f} MiB",
                       static_cast<double>(n) / static_cast<double>(kMiB));
  }
  if (n >= kKiB) {
    return std::format("{:.1f} KiB",
                       static_cast<double>(n) / static_cast<double>(kKiB));
  }
  return std::format("{} B", n);
}

std::string format_arena_bytes(size_t n) {
  if (n == static_cast<size_t>(-1)) {
    return "unlimited";
  }
  return format_mem_bytes(static_cast<uint64_t>(n));
}

}  // namespace

void DiagnosticToolsPanel::sample_memory() {
  base::sample_memory_counters_to_process_trace();
}

void DiagnosticToolsPanel::push_memory_sample(bool schedule) {
  const base::trace::ProcessMemorySnapshot os =
      base::trace::process_memory_snapshot();
  size_t tls_used = 0;
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls_used = tls->used();
  }
  if (auto* mem = static_cast<MemoryPageView*>(memory_page_)) {
    mem->push_sample(static_cast<int64_t>(os.working_set),
                     static_cast<int64_t>(tls_used),
                     static_cast<int64_t>(base::AllocationTracker::live_bytes()),
                     schedule);
    mem->set_flame(base::AllocationTracker::flame_snapshot(),
                   base::AllocationTracker::is_enabled(), schedule);
  }
}

void DiagnosticToolsPanel::refresh_memory_stats() {
  if (!memory_stats_) {
    return;
  }
  const base::trace::ProcessMemorySnapshot os =
      base::trace::process_memory_snapshot();
  size_t arena_used = 0;
  size_t arena_cap = 0;
  size_t tls_used = 0;
  if (base::MemoryResource* mr = base::memory_resource()) {
    arena_used = mr->used();
    arena_cap = mr->capacity();
  }
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls_used = tls->used();
  }
  memory_stats_->set_text(std::format(
      "ws {} (peak {}) private {} / phys {} | arena {} / {} | tls {} | "
      "tracker live {} peak {} ({})",
      format_mem_bytes(os.working_set),
      format_mem_bytes(os.peak_working_set),
      format_mem_bytes(os.private_bytes), format_mem_bytes(os.physical_total),
      format_arena_bytes(arena_used), format_arena_bytes(arena_cap),
      format_mem_bytes(tls_used),
      format_mem_bytes(base::AllocationTracker::live_bytes()),
      format_mem_bytes(base::AllocationTracker::peak_bytes()),
      base::AllocationTracker::is_enabled() ? "on" : "off"));
}

void DiagnosticToolsPanel::update_status() {
  // set_checked does not fire the change callback, so this only mirrors
  // process state (always-on starts after the panel is constructed).
  if (arm_) {
    arm_->set_checked(base::trace::tracing_enabled());
  }
  if (track_allocs_) {
    track_allocs_->set_checked(base::AllocationTracker::is_enabled());
  }
  if (!status_) {
    return;
  }
  // Keep one short line — long status preferred-width used to starve tabs_host.
  status_->set_text(std::format(
      "{} · {} events",
      base::trace::tracing_enabled() ? "Recording" : "Stopped",
      base::trace::process_trace().size()));
}

void DiagnosticToolsPanel::maybe_auto_refresh() {
  if (!visible_) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  if (now - last_auto_refresh_ < std::chrono::milliseconds(400)) {
    return;
  }
  last_auto_refresh_ = now;
  if (cpu_) {
    cpu_->refresh_from_process_trace(/*schedule=*/false);
  }
  push_memory_sample(/*schedule=*/true);
  refresh_memory_stats();
  update_status();
}

void DiagnosticToolsPanel::arm_memory_timer() {
  if (!visible_ || !alive_.load() || memory_timer_armed_) {
    return;
  }
  HWND hwnd = widget() ? widget()->hwnd() : nullptr;
  if (!hwnd) {
    return;
  }
  if (SetTimer(hwnd, reinterpret_cast<UINT_PTR>(this), 500,
               &DiagnosticToolsPanel::on_memory_timer)) {
    memory_timer_armed_ = true;
  }
}

void DiagnosticToolsPanel::disarm_memory_timer() {
  memory_timer_armed_ = false;
  HWND hwnd = widget() ? widget()->hwnd() : nullptr;
  if (hwnd) {
    KillTimer(hwnd, reinterpret_cast<UINT_PTR>(this));
  }
}

void CALLBACK DiagnosticToolsPanel::on_memory_timer(HWND, UINT, UINT_PTR id,
                                                   DWORD) {
  auto* self = reinterpret_cast<DiagnosticToolsPanel*>(id);
  if (!self || !self->alive_.load()) {
    return;
  }
  self->maybe_auto_refresh();
}

}  // namespace views
}  // namespace ui
