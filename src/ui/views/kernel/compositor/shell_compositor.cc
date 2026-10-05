// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/compositor/shell_compositor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/raster/paint_stats.h"

#include "base/trace/event/process_trace.h"

namespace ui {
namespace views {
namespace {

// Matches paint_schedule is_large_dirty (area > client/5) inverted: hover-sized
// dirty must not take the full-DIB swap path when a matching front exists.
bool is_small_raster_dirty(const Rect& dirty, int width_px, int height_px) {
  if (dirty.width <= 0 || dirty.height <= 0 || width_px <= 0 || height_px <= 0) {
    return false;
  }
  const std::int64_t dirty_area = static_cast<std::int64_t>(dirty.width) *
                                  static_cast<std::int64_t>(dirty.height);
  const std::int64_t frame_area =
      static_cast<std::int64_t>(width_px) * static_cast<std::int64_t>(height_px);
  return dirty_area * 5 < frame_area;
}

Rect clamp_raster_dirty(Rect dirty, int width_px, int height_px) {
  if (dirty.width <= 0 || dirty.height <= 0) {
    return Rect{0, 0, width_px, height_px};
  }
  if (dirty.x < 0) {
    dirty.width += dirty.x;
    dirty.x = 0;
  }
  if (dirty.y < 0) {
    dirty.height += dirty.y;
    dirty.y = 0;
  }
  if (dirty.x + dirty.width > width_px) {
    dirty.width = width_px - dirty.x;
  }
  if (dirty.y + dirty.height > height_px) {
    dirty.height = height_px - dirty.y;
  }
  if (dirty.width < 0) {
    dirty.width = 0;
  }
  if (dirty.height < 0) {
    dirty.height = 0;
  }
  return dirty;
}

bool dirty_covers_frame(const Rect& dirty, int width_px, int height_px) {
  return dirty.x <= 0 && dirty.y <= 0 && dirty.width >= width_px &&
         dirty.height >= height_px;
}

}  // namespace

ShellCompositor::ShellCompositor() = default;

ShellCompositor::~ShellCompositor() {
  shutdown();
}

void ShellCompositor::start() {
  std::lock_guard<std::mutex> lock(mu_);
  if (started_) {
    return;
  }
  stop_ = false;
  started_ = true;
  worker_ = std::thread([this] { worker_main(); });
}

void ShellCompositor::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    // Drop wake target before join so a finishing raster cannot PostMessage
    // into a HWND the owner is about to DestroyWindow.
    wake_hwnd_ = nullptr;
    wake_gen_ = 0;
    last_wake_posted_gen_ = 0;
    if (!started_) {
      release_dib(&buffers_[0]);
      release_dib(&buffers_[1]);
      if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
        font_px_ = 0;
      }
      return;
    }
    stop_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
  }
  std::lock_guard<std::mutex> lock(mu_);
  started_ = false;
  has_pending_ = false;
  has_active_ = false;
  published_gen_ = 0;
  wake_hwnd_ = nullptr;
  wake_gen_ = 0;
  last_wake_posted_gen_ = 0;
  release_dib(&buffers_[0]);
  release_dib(&buffers_[1]);
  if (font_) {
    DeleteObject(font_);
    font_ = nullptr;
    font_px_ = 0;
  }
}

void ShellCompositor::commit(PaintCommit frame) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    pending_ = std::move(frame);
    has_pending_ = true;
    ui::gfx::note_commit();
  }
  cv_.notify_one();
}

void ShellCompositor::notify_when_published(std::uint64_t generation,
                                            HWND hwnd) {
  HWND post_now = nullptr;
  std::uint64_t post_gen = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stop_ || !hwnd || generation == 0) {
      wake_hwnd_ = nullptr;
      wake_gen_ = 0;
      return;
    }
    wake_hwnd_ = hwnd;
    wake_gen_ = generation;
    // Already published (fast worker / coalesced): post once from UI thread.
    if (published_gen_ >= generation &&
        published_gen_ > last_wake_posted_gen_) {
      post_now = wake_hwnd_;
      post_gen = published_gen_;
      last_wake_posted_gen_ = published_gen_;
      wake_hwnd_ = nullptr;
      wake_gen_ = 0;
    }
  }
  if (post_now) {
    PostMessageW(post_now, kShellPublishedMessage,
                 static_cast<WPARAM>(post_gen), 0);
  }
}

bool ShellCompositor::wait_published(std::uint64_t generation) {
  std::unique_lock<std::mutex> lock(mu_);
  cv_.wait(lock, [&] {
    return stop_ || published_gen_ >= generation;
  });
  return !stop_ && published_gen_ >= generation;
}

// Caller holds mu_. Returns hwnd to PostMessage after unlock (or nullptr).
HWND ShellCompositor::maybe_take_wake_hwnd_locked(std::uint64_t generation) {
  if (stop_ || !wake_hwnd_ || generation < wake_gen_ ||
      generation <= last_wake_posted_gen_) {
    return nullptr;
  }
  HWND hwnd = wake_hwnd_;
  wake_hwnd_ = nullptr;
  wake_gen_ = 0;
  last_wake_posted_gen_ = generation;
  return hwnd;
}

std::uint64_t ShellCompositor::present(HDC hdc,
                                       const RECT& dest,
                                       ui::gfx::Color fallback_fill) {
  BASE_TRACE_EVENT("blt_present", "ui.views");
  if (!hdc) {
    return 0;
  }
  const int blt_x = dest.left;
  const int blt_y = dest.top;
  const int blt_w = dest.right - dest.left;
  const int blt_h = dest.bottom - dest.top;
  if (blt_w <= 0 || blt_h <= 0) {
    return 0;
  }

  LARGE_INTEGER t0 = {};
  QueryPerformanceCounter(&t0);

  // BitBlt first, then fill only uncovered margins. Filling the whole |dest|
  // before the blit flashed shell_bg on every mouse-move WM_PAINT (NULL_BRUSH
  // + no erase) and looked like hollow self-drawn chrome. Resize/move still
  // needs opaque fill where the published front lags the client size.
  std::uint64_t presented_gen = 0;
  int copied_w = 0;
  int copied_h = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    Dib& front = buffers_[front_];
    if (front.dc && front.bits && front.w > 0 && front.h > 0) {
      const int src_x = blt_x;
      const int src_y = blt_y;
      if (src_x < front.w && src_y < front.h) {
        const int copy_w =
            (std::min)(blt_w, front.w - src_x);
        const int copy_h =
            (std::min)(blt_h, front.h - src_y);
        if (copy_w > 0 && copy_h > 0 &&
            BitBlt(hdc, blt_x, blt_y, copy_w, copy_h, front.dc, src_x, src_y,
                   SRCCOPY) != FALSE) {
          // Capture under the same lock as the blit so OnShellPublished cannot
          // observe a newer published_gen_ than the pixels just shown.
          presented_gen = published_gen_;
          copied_w = copy_w;
          copied_h = copy_h;
        }
      }
    }
  }

  if (copied_w < blt_w || copied_h < blt_h) {
    const std::uint8_t fill_r =
        static_cast<std::uint8_t>((fallback_fill >> 16) & 0xff);
    const std::uint8_t fill_g =
        static_cast<std::uint8_t>((fallback_fill >> 8) & 0xff);
    const std::uint8_t fill_b =
        static_cast<std::uint8_t>(fallback_fill & 0xff);
    HBRUSH brush = CreateSolidBrush(RGB(fill_r, fill_g, fill_b));
    if (brush) {
      if (copied_w <= 0 || copied_h <= 0) {
        RECT fill_rc = dest;
        FillRect(hdc, &fill_rc, brush);
      } else {
        if (copied_w < blt_w) {
          RECT right = {blt_x + copied_w, blt_y, blt_x + blt_w,
                        blt_y + blt_h};
          FillRect(hdc, &right, brush);
        }
        if (copied_h < blt_h) {
          RECT bottom = {blt_x, blt_y + copied_h, blt_x + copied_w,
                         blt_y + blt_h};
          FillRect(hdc, &bottom, brush);
        }
      }
      DeleteObject(brush);
    }
  }

  LARGE_INTEGER t1 = {};
  QueryPerformanceCounter(&t1);
  if (t1.QuadPart > t0.QuadPart) {
    ui::gfx::note_present_qpc(
        static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
  }
  return presented_gen;
}

ui::gfx::ShellRaster ShellCompositor::shell_raster() const {
  std::lock_guard<std::mutex> lock(mu_);
  ui::gfx::ShellRaster shell;
  const Dib& front = buffers_[front_];
  if (!front.bits || front.w <= 0 || front.h <= 0) {
    return shell;
  }
  shell.bgra = static_cast<const std::uint8_t*>(front.bits);
  shell.width_px = static_cast<std::uint32_t>(front.w);
  shell.height_px = static_cast<std::uint32_t>(front.h);
  shell.stride_bytes = shell.width_px * 4u;
  return shell;
}

bool ShellCompositor::copy_published_shell(
    std::vector<std::uint8_t>* out_bgra,
    ui::gfx::ShellRaster* out_meta,
    std::uint64_t* out_generation) const {
  if (!out_bgra || !out_meta) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  const Dib& front = buffers_[front_];
  if (!front.bits || front.w <= 0 || front.h <= 0) {
    out_bgra->clear();
    *out_meta = {};
    if (out_generation) {
      *out_generation = 0;
    }
    return false;
  }
  const std::size_t bytes = static_cast<std::size_t>(front.w) *
                            static_cast<std::size_t>(front.h) * 4u;
  out_bgra->resize(bytes);
  std::memcpy(out_bgra->data(), front.bits, bytes);
  out_meta->bgra = out_bgra->data();
  out_meta->width_px = static_cast<std::uint32_t>(front.w);
  out_meta->height_px = static_cast<std::uint32_t>(front.h);
  out_meta->stride_bytes = out_meta->width_px * 4u;
  if (out_generation) {
    *out_generation = published_gen_;
  }
  return true;
}

std::uint64_t ShellCompositor::published_generation() const {
  std::lock_guard<std::mutex> lock(mu_);
  return published_gen_;
}

void ShellCompositor::front_buffer_size(int* width_px, int* height_px) const {
  std::lock_guard<std::mutex> lock(mu_);
  const Dib& front = buffers_[front_];
  if (width_px) {
    *width_px = front.bits ? front.w : 0;
  }
  if (height_px) {
    *height_px = front.bits ? front.h : 0;
  }
}

void ShellCompositor::release_buffers() {
  std::lock_guard<std::mutex> lock(mu_);
  ui::gfx::Canvas::discard_retained_surface();
  release_dib(&buffers_[0]);
  release_dib(&buffers_[1]);
  published_gen_ = 0;
}

void ShellCompositor::worker_main() {
  for (;;) {
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [&] { return stop_ || has_pending_; });
      if (stop_) {
        break;
      }
      activate_pending();
    }
    raster_active();
  }
}

void ShellCompositor::activate_pending() {
  // Caller holds mu_.
  active_ = std::move(pending_);
  has_pending_ = false;
  has_active_ = true;
  ui::gfx::note_activate();
}

void ShellCompositor::raster_active() {
  BASE_TRACE_EVENT("raster", "ui.views");
  PaintCommit frame;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!has_active_) {
      return;
    }
    frame = active_;
  }

  LARGE_INTEGER t0 = {};
  QueryPerformanceCounter(&t0);

  HDC back_dc = nullptr;
  int back_w = 0;
  int back_h = 0;
  Rect dirty{};
  bool subset_publish = false;
  HFONT font = nullptr;
  HGDIOBJ old_font = nullptr;
  HWND wake_hwnd = nullptr;
  std::uint64_t wake_gen = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stop_) {
      return;
    }
    const Dib& published = buffers_[front_];
    // Same generation already on a matching front: hover coalesces must not
    // recreate DIBs or replay the full shell.
    if (frame.generation != 0 && frame.generation <= published_gen_ &&
        published.bits && published.w == frame.width_px &&
        published.h == frame.height_px) {
      wake_hwnd = maybe_take_wake_hwnd_locked(published_gen_);
      wake_gen = published_gen_;
    } else {
      if (!ensure_dib(&buffers_[back_], frame.width_px, frame.height_px)) {
        return;
      }
      Dib& back = buffers_[back_];
      Dib& front = buffers_[front_];
      font = ensure_font_locked(frame.font_px);
      old_font = font ? SelectObject(back.dc, font) : nullptr;

      dirty = clamp_raster_dirty(frame.dirty, frame.width_px, frame.height_px);
      // Geometric full-DIB and not hover-sized: swap buffers. Area < 1/5 of
      // the frame stays a subset when the front matches.
      const bool full_swap =
          dirty_covers_frame(dirty, frame.width_px, frame.height_px) &&
          !is_small_raster_dirty(dirty, frame.width_px, frame.height_px);
      if (!full_swap && front.dc && front.bits && front.w == frame.width_px &&
          front.h == frame.height_px) {
        // Keep the published front; raster only |dirty| into the back scratch
        // and copy that rect over. Do not BitBlt the whole shell.
        subset_publish = true;
      } else if (!full_swap) {
        dirty = Rect{0, 0, frame.width_px, frame.height_px};
      }
      back_dc = back.dc;
      back_w = back.w;
      back_h = back.h;
    }
  }

  if (wake_hwnd) {
    PostMessageW(wake_hwnd, kShellPublishedMessage,
                 static_cast<WPARAM>(wake_gen), 0);
    LARGE_INTEGER t1 = {};
    QueryPerformanceCounter(&t1);
    if (t1.QuadPart > t0.QuadPart) {
      ui::gfx::note_raster_qpc(
          static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
    }
    cv_.notify_all();
    return;
  }

  // Raster without mu_ so present() can BitBlt the published front.
  if (back_dc && dirty.width > 0 && dirty.height > 0) {
    raster_dirty_into(back_dc, back_w, back_h, frame, dirty, font);
  }

  wake_hwnd = nullptr;
  wake_gen = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (font && back_dc) {
      SelectObject(back_dc, old_font);
    }
    if (!stop_) {
      if (subset_publish) {
        copy_dib_rect(&buffers_[front_], buffers_[back_], dirty);
      } else {
        std::swap(front_, back_);
      }
      published_gen_ = frame.generation;
      wake_hwnd = maybe_take_wake_hwnd_locked(published_gen_);
      wake_gen = published_gen_;
    }
  }

  if (wake_hwnd) {
    // Wake UI thread only — never BeginPaint / DestroyWindow from worker.
    PostMessageW(wake_hwnd, kShellPublishedMessage,
                 static_cast<WPARAM>(wake_gen), 0);
  }

  LARGE_INTEGER t1 = {};
  QueryPerformanceCounter(&t1);
  if (t1.QuadPart > t0.QuadPart) {
    ui::gfx::note_raster_qpc(
        static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
  }
  cv_.notify_all();
}

void ShellCompositor::raster_dirty_into(HDC dc,
                                        int dib_w,
                                        int dib_h,
                                        const PaintCommit& frame,
                                        Rect dirty,
                                        HFONT font) {
  if (!dc || dirty.width <= 0 || dirty.height <= 0 || dib_w <= 0 || dib_h <= 0) {
    return;
  }
  const int clip_l = (std::max)(0, dirty.x);
  const int clip_t = (std::max)(0, dirty.y);
  const int clip_r = (std::min)(dib_w, dirty.x + dirty.width);
  const int clip_b = (std::min)(dib_h, dirty.y + dirty.height);
  if (clip_r <= clip_l || clip_b <= clip_t) {
    return;
  }
  HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
  ui::gfx::Canvas canvas(dc, dib_w, dib_h);
  // save/restore: Skia/GDI clips must not shrink across hover frames.
  canvas.save();
  canvas.clip_rect(clip_l, clip_t, clip_r - clip_l, clip_b - clip_t);
  canvas.fill_rect(clip_l, clip_t, clip_r - clip_l, clip_b - clip_t,
                   frame.clear_color);
  // Never unclipped replay: skip cmds that miss dirty intersect DIB (hover
  // chrome and tall TableView rows below the window).
  frame.display_list.replay_clipped(&canvas, clip_l, clip_t, clip_r, clip_b);
  canvas.restore();
  const std::uint64_t pixels = static_cast<std::uint64_t>(clip_r - clip_l) *
                               static_cast<std::uint64_t>(clip_b - clip_t);
  ui::gfx::note_paint_area(pixels, pixels);
  if (old) {
    SelectObject(dc, old);
  }
}

void ShellCompositor::copy_dib_rect(Dib* dst, const Dib& src, Rect r) {
  if (!dst || !dst->bits || !src.bits || r.width <= 0 || r.height <= 0) {
    return;
  }
  const int x0 = (std::max)(0, r.x);
  const int y0 = (std::max)(0, r.y);
  const int x1 = (std::min)(r.x + r.width, (std::min)(dst->w, src.w));
  const int y1 = (std::min)(r.y + r.height, (std::min)(dst->h, src.h));
  if (x1 <= x0 || y1 <= y0) {
    return;
  }
  const std::size_t bytes = static_cast<std::size_t>(x1 - x0) * 4u;
  auto* d = static_cast<std::uint8_t*>(dst->bits);
  const auto* s = static_cast<const std::uint8_t*>(src.bits);
  const std::size_t d_stride = static_cast<std::size_t>(dst->w) * 4u;
  const std::size_t s_stride = static_cast<std::size_t>(src.w) * 4u;
  const std::size_t x_off = static_cast<std::size_t>(x0) * 4u;
  for (int y = y0; y < y1; ++y) {
    const std::size_t row = static_cast<std::size_t>(y);
    std::memcpy(d + row * d_stride + x_off, s + row * s_stride + x_off, bytes);
  }
}

bool ShellCompositor::ensure_dib(Dib* dib, int width_px, int height_px) {
  if (!dib || width_px <= 0 || height_px <= 0) {
    return false;
  }
  if (dib->dc && dib->dib && dib->w == width_px && dib->h == height_px) {
    return true;
  }
  release_dib(dib);

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width_px;
  bmi.bmiHeader.biHeight = -height_px;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP bitmap =
      CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!bitmap || !bits) {
    if (bitmap) {
      DeleteObject(bitmap);
    }
    return false;
  }
  HDC mem = CreateCompatibleDC(nullptr);
  if (!mem) {
    DeleteObject(bitmap);
    return false;
  }
  dib->old = static_cast<HBITMAP>(SelectObject(mem, bitmap));
  dib->dc = mem;
  dib->dib = bitmap;
  dib->bits = bits;
  dib->w = width_px;
  dib->h = height_px;
  return true;
}

void ShellCompositor::release_dib(Dib* dib) {
  if (!dib) {
    return;
  }
  // Skia may wrap these bits; drop retained surfaces before DeleteObject.
  if (dib->dib) {
    ui::gfx::Canvas::discard_retained_surface();
  }
  if (dib->dc) {
    if (dib->old) {
      SelectObject(dib->dc, dib->old);
      dib->old = nullptr;
    }
    DeleteDC(dib->dc);
    dib->dc = nullptr;
  }
  if (dib->dib) {
    DeleteObject(dib->dib);
    dib->dib = nullptr;
  }
  dib->bits = nullptr;
  dib->w = 0;
  dib->h = 0;
}

HFONT ShellCompositor::ensure_font_locked(int height_px) {
  if (height_px < 8) {
    height_px = 8;
  }
  if (font_ && font_px_ == height_px) {
    return font_;
  }
  if (font_) {
    DeleteObject(font_);
    font_ = nullptr;
  }
  ui::gfx::note_create_font();
  font_ =
      CreateFontW(-height_px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  font_px_ = height_px;
  return font_;
}

}  // namespace views
}  // namespace ui
