// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/widget/widget.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>

#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/compositor/shell_compositor.h"
#include "ui/views/kernel/paint/paint_commit.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/paint_schedule.h"

#include "base/trace/event/process_trace.h"

namespace ui {
namespace views {

ui::gfx::ShellRaster Widget::shell_raster() const {
  if (!compositor_) {
    return {};
  }
  return compositor_->shell_raster();
}

bool Widget::copy_shell_raster(std::vector<std::uint8_t>* out_bgra,
                               ui::gfx::ShellRaster* out_meta,
                               std::uint64_t* out_generation) const {
  if (!compositor_) {
    return false;
  }
  return compositor_->copy_published_shell(out_bgra, out_meta, out_generation);
}

std::uint64_t Widget::shell_generation() const {
  if (!compositor_) {
    return 0;
  }
  return compositor_->published_generation();
}

void Widget::set_contents_view(std::unique_ptr<View> contents) {
  focused_ = nullptr;
  hovered_ = nullptr;
  pressed_ = nullptr;
  contents_ = std::move(contents);
  if (contents_) {
    // set_widget walks the tree and notifies each node for DPI != 1 so Label /
    // Button / MenuBar rebuild preferred sizes (do not also propagate here —
    // that would double-scale views that multiply preferred_size by ratio).
    contents_->set_widget(this);
    layout_contents();
    contents_->realize_native_tree();
  }
}

void Widget::set_device_scale_factor(float scale_factor) {
  if (scale_factor <= 0.f) {
    return;
  }
  const float old = device_scale_factor_;
  if (std::fabs(old - scale_factor) < 0.0001f) {
    return;
  }
  device_scale_factor_ = scale_factor;
  dpi_ = static_cast<unsigned>(
      std::lround(scale_factor * static_cast<float>(kDefaultDpi)));
  if (contents_) {
    contents_->propagate_device_scale_factor_changed(old, device_scale_factor_);
    layout_contents();
    schedule_paint();
  }
}

void Widget::sync_dpi_from_hwnd() {
  dpi_ = dpi_for_hwnd(hwnd_);
  device_scale_factor_ = scale_factor_from_dpi(dpi_);
}

void Widget::on_dpi_changed(unsigned new_dpi, const RECT* suggested) {
  if (new_dpi == 0) {
    return;
  }
  const float old_scale = device_scale_factor_;
  const float new_scale = scale_factor_from_dpi(new_dpi);
  dpi_ = new_dpi;
  device_scale_factor_ = new_scale;
  if (suggested && hwnd_) {
    SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left,
                 suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  }
  if (contents_ && old_scale > 0.f &&
      std::fabs(old_scale - new_scale) >= 0.0001f) {
    contents_->propagate_device_scale_factor_changed(old_scale, new_scale);
  }
  layout_contents();
  schedule_paint();
}

void Widget::layout_contents() {
  if (!contents_ || !hwnd_) {
    return;
  }
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  contents_->set_bounds({0, 0, rc.right - rc.left, rc.bottom - rc.top});
  // Drain follow-up marks from add_child / set_visible during a pass. Cap so a
  // pathological LayoutManager cannot spin the UI thread.
  constexpr int kMaxLayoutPasses = 8;
  int passes = 0;
  do {
    contents_->layout();
  } while (contents_->needs_layout() && ++passes < kMaxLayoutPasses);
  contents_->realize_native_tree();
  contents_->sync_native_tree();
}

void Widget::ensure_compositor_started() {
  if (!compositor_ || !contents_) {
    return;
  }
  compositor_->start();
}

void Widget::pump_until_shell_published(unsigned timeout_ms) {
  if (!hwnd_ || !IsWindow(hwnd_) || timeout_ms == 0) {
    return;
  }
  ensure_compositor_started();
  // Wait for a *new* publish when one already exists — otherwise tab chrome
  // (Map→3D) stays on a stale front buffer after schedule_paint (plain #6).
  const std::uint64_t before = shell_generation();
  schedule_paint();
  const DWORD t0 = GetTickCount();
  MSG msg = {};
  while (GetTickCount() - t0 < timeout_ms) {
    const std::uint64_t now = shell_generation();
    if (now > before) {
      return;
    }
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        PostQuitMessage(static_cast<int>(msg.wParam));
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    UpdateWindow(hwnd_);
    Sleep(5);
  }
}

void Widget::union_pending_dirty(const Rect& dirty) {
  if (dirty.width <= 0 || dirty.height <= 0) {
    return;
  }
  if (full_paint_pending_) {
    return;
  }
  pending_dirty_ = detail::union_dirty_rects(pending_dirty_, dirty);
}

void Widget::take_pending_dirty(int width_px, int height_px, Rect* out) {
  if (!out) {
    return;
  }
  if (full_paint_pending_ || pending_dirty_.width <= 0 ||
      pending_dirty_.height <= 0) {
    *out = Rect{0, 0, width_px, height_px};
  } else {
    *out = pending_dirty_;
  }
  pending_dirty_ = {};
  full_paint_pending_ = false;
}

bool Widget::has_pending_paint() const {
  return full_paint_pending_ ||
         (pending_dirty_.width > 0 && pending_dirty_.height > 0);
}

void Widget::schedule_paint() {
  full_paint_pending_ = true;
  pending_dirty_ = {};
  if (hwnd_ && IsWindow(hwnd_)) {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void Widget::schedule_paint_rect(const Rect& dirty) {
  if (!hwnd_ || !IsWindow(hwnd_)) {
    return;
  }
  if (dirty.width <= 0 || dirty.height <= 0) {
    return;
  }
  if (!full_paint_pending_) {
    union_pending_dirty(dirty);
  }
  RECT rc = {dirty.x, dirty.y, dirty.x + dirty.width, dirty.y + dirty.height};
  InvalidateRect(hwnd_, &rc, FALSE);
}

void Widget::on_theme_changed() {
  if (contents_) {
    std::function<void(View*)> dirty = [&](View* v) {
      if (!v) {
        return;
      }
      v->invalidate_commands();
      for (size_t i = 0; i < v->child_count(); ++i) {
        dirty(v->child_at(i));
      }
    };
    dirty(contents_.get());
  }
  schedule_paint();
}

void Widget::on_shell_published_message(std::uint64_t generation) {
  // Worker wake: coalesce InvalidateRect on the UI thread. Do not Commit or
  // Present here — the next WM_PAINT BitBlts the published front.
  if (destroying_ || will_close_fired_ || !hwnd_ || !IsWindow(hwnd_)) {
    return;
  }
  if (generation == 0 || generation < awaiting_publish_gen_) {
    return;
  }
  // Do not drop this wake. A WM_PAINT already inside BeginPaint may BitBlt the
  // previous front and clear the coalesce flag, so swallowing the publish
  // leaves the new rect stale until a hover paint.
  shell_wake_invalidate_pending_ = true;
  if (awaiting_publish_dirty_.width > 0 && awaiting_publish_dirty_.height > 0) {
    RECT rc = {awaiting_publish_dirty_.x, awaiting_publish_dirty_.y,
               awaiting_publish_dirty_.x + awaiting_publish_dirty_.width,
               awaiting_publish_dirty_.y + awaiting_publish_dirty_.height};
    InvalidateRect(hwnd_, &rc, FALSE);
  } else {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void Widget::maybe_notify_shell_published(std::uint64_t published_gen) {
  if (!on_shell_published_ || !*on_shell_published_) {
    return;
  }
  if (published_gen == 0 || published_gen < awaiting_publish_gen_ ||
      published_gen == last_shell_published_notified_) {
    return;
  }
  last_shell_published_notified_ = published_gen;
  (*on_shell_published_)(awaiting_publish_dirty_);
}

void Widget::on_paint() {
  BASE_TRACE_EVENT("on_paint", "ui.views");
  // One-shot startup span for the first shell WM_PAINT (full path duration).
  static bool s_first_shell_paint = true;
  std::optional<::base::trace::ScopedTraceEvent> first_paint;
  if (s_first_shell_paint) {
    s_first_shell_paint = false;
    first_paint.emplace("FirstShellPaint", "startup");
  }
  LARGE_INTEGER t0 = {};
  QueryPerformanceCounter(&t0);

  PAINTSTRUCT ps = {};
  HDC hdc = BeginPaint(hwnd_, &ps);
  shell_wake_invalidate_pending_ = false;
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  ensure_compositor_started();
  if (w <= 0 || h <= 0 || IsIconic(hwnd_) || !compositor_) {
    if (compositor_ && (w <= 0 || h <= 0 || IsIconic(hwnd_))) {
      compositor_->release_buffers();
    }
    EndPaint(hwnd_, &ps);
    return;
  }

  if (contents_ && contents_->needs_layout()) {
    BASE_TRACE_EVENT("layout", "ui.views");
    layout_contents();
  }

  // UI thread: record + Commit immutable DisplayList when horizon dirtied the
  // HWND. Do NOT wait_published — that blocked hover on every WM_PAINT.
  // Present the last published front immediately; the worker posts
  // kShellPublishedMessage when a newer generation is ready, and the next
  // paint BitBlts it. Wake-only paints (no pending dirty) skip Commit so a
  // publish notify cannot re-record the whole tree.
  Rect committed_dirty{};
  bool did_commit = false;
  if (contents_ && has_pending_paint()) {
    // U5: coalesce rapid Commits to ~display refresh (skip record when the
    // previous Commit is still within one frame). Still present + keep dirty
    // so the next tick records the unioned region.
    // Never coalesce when the published front lags the client — NULL_BRUSH +
    // a smaller front leaves desktop show-through / stale horizon after resize.
    int front_w = 0;
    int front_h = 0;
    compositor_->front_buffer_size(&front_w, &front_h);
    const bool front_lags_client =
        front_w <= 0 || front_h <= 0 || front_w < w || front_h < h;
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    const std::int64_t frame_ticks = detail::refresh_frame_ticks();
    const bool large_dirty = detail::is_large_dirty(pending_dirty_,
                                                    full_paint_pending_, w, h);
    const bool within_frame = detail::should_coalesce_commit(
        front_lags_client, large_dirty, now.QuadPart, last_commit_qpc_,
        frame_ticks);
    if (within_frame) {
      // Keep pending dirty; present last front. Arm a one-shot timer so the
      // last hover in this refresh window still Commits. Do not InvalidateRect
      // here — that re-enters paint while still within_frame and spins.
      SetTimer(hwnd_, static_cast<UINT_PTR>(detail::kShellCommitCoalesceTimer),
               16, nullptr);
    } else {
      KillTimer(hwnd_, static_cast<UINT_PTR>(detail::kShellCommitCoalesceTimer));
      take_pending_dirty(w, h, &committed_dirty);
      PaintCommit frame;
      const int font_px = shell_body_font_px(device_scale_factor_);
      {
        BASE_TRACE_EVENT("record_commit", "ui.views");
        if (commit_view_tree(contents_.get(), committed_dirty, w, h, font_px,
                             Theme::current().shell_bg, &frame)) {
          awaiting_publish_gen_ = frame.generation;
          awaiting_publish_dirty_ = committed_dirty;
          compositor_->commit(std::move(frame));
          compositor_->notify_when_published(awaiting_publish_gen_, hwnd_);
          did_commit = true;
          last_commit_qpc_ = now.QuadPart;
          last_begin_frame_qpc_ = now.QuadPart;
          ui::gfx::note_begin_frame_qpc(
              static_cast<std::uint64_t>(now.QuadPart));
        }
      }
    }
  }

  // Layout / set_bounds may expand dirty beyond BeginPaint's update region.
  // BitBlt the union so newly exposed horizon is not left blank until hover.
  RECT blit = ps.rcPaint;
  if (did_commit && committed_dirty.width > 0 && committed_dirty.height > 0) {
    const int l =
        committed_dirty.x < blit.left ? committed_dirty.x : blit.left;
    const int t =
        committed_dirty.y < blit.top ? committed_dirty.y : blit.top;
    const int r = committed_dirty.right() > blit.right ? committed_dirty.right()
                                                       : blit.right;
    const int b = committed_dirty.bottom() > blit.bottom
                      ? committed_dirty.bottom()
                      : blit.bottom;
    blit = {l, t, r, b};
  }
  std::uint64_t presented_gen = 0;
  {
    BASE_TRACE_EVENT("present", "ui.views");
    presented_gen =
        compositor_->present(hdc, blit, Theme::current().shell_bg);
  }
  EndPaint(hwnd_, &ps);

  if (did_commit && last_begin_frame_qpc_ != 0) {
    LARGE_INTEGER t_present = {};
    QueryPerformanceCounter(&t_present);
    if (t_present.QuadPart > last_begin_frame_qpc_) {
      ui::gfx::note_begin_frame_to_shell_present_qpc(static_cast<std::uint64_t>(
          t_present.QuadPart - last_begin_frame_qpc_));
    }
  }

  // Commit is async: union blit may still be the previous front. Force a
  // follow-up paint for any committed area outside the original update rect
  // so wake/hover is not the only path that refreshes newly exposed horizon.
  if (did_commit && committed_dirty.width > 0 && committed_dirty.height > 0) {
    const bool expands =
        committed_dirty.x < ps.rcPaint.left ||
        committed_dirty.y < ps.rcPaint.top ||
        committed_dirty.right() > ps.rcPaint.right ||
        committed_dirty.bottom() > ps.rcPaint.bottom;
    if (expands) {
      RECT rc = {committed_dirty.x, committed_dirty.y, committed_dirty.right(),
                 committed_dirty.bottom()};
      InvalidateRect(hwnd_, &rc, FALSE);
    }
  }

  maybe_notify_shell_published(presented_gen);

  const int area_w = ps.rcPaint.right - ps.rcPaint.left;
  const int area_h = ps.rcPaint.bottom - ps.rcPaint.top;
  ui::gfx::note_paint_area(
      static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h),
      area_w > 0 && area_h > 0
          ? static_cast<std::uint64_t>(area_w) *
                static_cast<std::uint64_t>(area_h)
          : 0);
  LARGE_INTEGER t1 = {};
  QueryPerformanceCounter(&t1);
  if (t1.QuadPart > t0.QuadPart) {
    ui::gfx::note_widget_paint_qpc(
        static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
  }
}

void Widget::on_size(int width, int height) {
  if (width <= 0 || height <= 0 || (hwnd_ && IsIconic(hwnd_))) {
    if (compositor_) {
      compositor_->release_buffers();
    }
  }
  layout_contents();
  // Break U5 coalesce so the next WM_PAINT must record at the new client size
  // (otherwise a hover Commit within ~16ms keeps presenting the old front).
  last_commit_qpc_ = 0;
  // Force a full shell Commit+Present. Setting full_paint_pending alone is not
  // enough: without InvalidateRect, resize/move can leave WS_CLIPCHILDREN holes
  // and a lagging front DIB unpainted (desktop show-through / overlap).
  schedule_paint();
  if (hwnd_ && IsWindow(hwnd_) && width > 0 && height > 0) {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

}  // namespace views
}  // namespace ui
