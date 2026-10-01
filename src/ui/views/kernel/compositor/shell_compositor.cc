// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/compositor/shell_compositor.h"

#include <algorithm>
#include <thread>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/raster/paint_stats.h"

#include "base/trace/event/process_trace.h"

namespace ui {
namespace views {

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

std::uint64_t ShellCompositor::published_generation() const {
  std::lock_guard<std::mutex> lock(mu_);
  return published_gen_;
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
  bool full_frame = true;
  HFONT font = nullptr;
  HGDIOBJ old_font = nullptr;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stop_) {
      return;
    }
    if (!ensure_dib(&buffers_[back_], frame.width_px, frame.height_px)) {
      return;
    }
    Dib& back = buffers_[back_];
    Dib& front = buffers_[front_];
    font = ensure_font_locked(frame.font_px);
    old_font = font ? SelectObject(back.dc, font) : nullptr;

    dirty = frame.dirty;
    if (dirty.width <= 0 || dirty.height <= 0) {
      dirty = Rect{0, 0, frame.width_px, frame.height_px};
    }
    if (dirty.x < 0) {
      dirty.width += dirty.x;
      dirty.x = 0;
    }
    if (dirty.y < 0) {
      dirty.height += dirty.y;
      dirty.y = 0;
    }
    if (dirty.x + dirty.width > frame.width_px) {
      dirty.width = frame.width_px - dirty.x;
    }
    if (dirty.y + dirty.height > frame.height_px) {
      dirty.height = frame.height_px - dirty.y;
    }
    full_frame = dirty.x <= 0 && dirty.y <= 0 &&
                 dirty.width >= frame.width_px &&
                 dirty.height >= frame.height_px;
    bool seeded = false;
    if (!full_frame && front.dc && front.bits && front.w == frame.width_px &&
        front.h == frame.height_px) {
      BitBlt(back.dc, 0, 0, frame.width_px, frame.height_px, front.dc, 0, 0,
             SRCCOPY);
      seeded = true;
      ui::gfx::Canvas::discard_retained_surface();
    }
    if (!full_frame && !seeded) {
      dirty = Rect{0, 0, frame.width_px, frame.height_px};
      full_frame = true;
    }
    back_dc = back.dc;
    back_w = back.w;
    back_h = back.h;
  }

  // Raster without mu_ so present() can BitBlt the previous front and U4
  // helper threads are not joined while holding the compositor lock.
  if (back_dc && dirty.width > 0 && dirty.height > 0) {
    raster_dirty_into(back_dc, back_w, back_h, frame, dirty, full_frame, font);
  }

  HWND wake_hwnd = nullptr;
  std::uint64_t wake_gen = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (font && back_dc) {
      SelectObject(back_dc, old_font);
    }
    if (!stop_) {
      std::swap(front_, back_);
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
                                        bool full_frame,
                                        HFONT font) {
  if (!dc || dirty.width <= 0 || dirty.height <= 0) {
    return;
  }
  // U4: large dirty -> two horizontal strips on temp DIBs, then BitBlt.
  constexpr int kMinParallelHeight = 256;
  constexpr int kMinParallelArea = 1280 * 400;
  const int area = dirty.width * dirty.height;
  const bool parallel = dirty.height >= kMinParallelHeight &&
                        area >= kMinParallelArea && dib_w > 0 && dib_h > 0;
  const int face_px = frame.font_px > 0 ? frame.font_px : 12;
  auto paint_strip = [&](HDC target, const Rect& strip, bool whole) {
    // Temp strip DCs are fresh CreateCompatibleDC — without a selected face
    // TextOut uses SYSTEM (~12px) while layout already reserved shell metrics.
    // Own a per-strip HFONT so U4 helper threads never share one GDI object.
    HFONT strip_font = nullptr;
    ui::gfx::note_create_font();
    strip_font =
        CreateFontW(-face_px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    const HFONT use = strip_font ? strip_font : font;
    HGDIOBJ old = use ? SelectObject(target, use) : nullptr;
    ui::gfx::Canvas canvas(target, dib_w, dib_h);
    canvas.fill_rect(strip.x, strip.y, strip.width, strip.height,
                     frame.clear_color);
    if (whole) {
      frame.display_list.replay(&canvas);
    } else {
      frame.display_list.replay_clipped(&canvas, strip.x, strip.y,
                                        strip.x + strip.width,
                                        strip.y + strip.height);
    }
    if (old) {
      SelectObject(target, old);
    }
    if (strip_font) {
      DeleteObject(strip_font);
    }
  };
  if (!parallel) {
    paint_strip(dc, dirty, full_frame);
    return;
  }

  const int mid_y = dirty.y + dirty.height / 2;
  Rect top{dirty.x, dirty.y, dirty.width, mid_y - dirty.y};
  Rect bottom{dirty.x, mid_y, dirty.width, dirty.bottom() - mid_y};

  Dib top_dib;
  Dib bottom_dib;
  auto fill_temp = [&](Dib* dib, const Rect& strip) {
    if (!dib || strip.width <= 0 || strip.height <= 0) {
      return;
    }
    if (!ensure_dib(dib, frame.width_px, frame.height_px)) {
      return;
    }
    paint_strip(dib->dc, strip, false);
  };

  std::thread helper([&] { fill_temp(&bottom_dib, bottom); });
  fill_temp(&top_dib, top);
  helper.join();

  if (top_dib.dc && top.width > 0 && top.height > 0) {
    BitBlt(dc, top.x, top.y, top.width, top.height, top_dib.dc, top.x, top.y,
           SRCCOPY);
  }
  if (bottom_dib.dc && bottom.width > 0 && bottom.height > 0) {
    BitBlt(dc, bottom.x, bottom.y, bottom.width, bottom.height, bottom_dib.dc,
           bottom.x, bottom.y, SRCCOPY);
  }
  release_dib(&top_dib);
  release_dib(&bottom_dib);
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
