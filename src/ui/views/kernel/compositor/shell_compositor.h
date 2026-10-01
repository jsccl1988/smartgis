// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_COMPOSITOR_SHELL_COMPOSITOR_H_
#define UI_VIEWS_KERNEL_COMPOSITOR_SHELL_COMPOSITOR_H_

#include "ui/ui_export.h"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/raster/shell_raster.h"
#include "ui/gfx/color/color.h"
#include "ui/views/kernel/paint/paint_commit.h"

namespace ui {
namespace views {

// Posted to the HWND registered via notify_when_published after that
// generation is published. wParam = generation (std::uint64_t truncated to
// WPARAM). UI thread must coalesce InvalidateRect; worker never BeginPaint.
inline constexpr UINT kShellPublishedMessage = WM_APP + 0x5343;  // 'SC'

// Process-local shell compositor: UI thread Commits into pending; one worker
// Activates, rasters DisplayList into a back DIB, and publishes a generation.
// WM_PAINT only BitBlts the published front buffer (never waits on the worker).
// After publish the worker may PostMessage kShellPublishedMessage to a
// registered HWND (wake only — no BeginPaint / DestroyWindow).
class UI_EXPORT ShellCompositor {
 public:
  ShellCompositor();
  ~ShellCompositor();

  ShellCompositor(const ShellCompositor&) = delete;
  ShellCompositor& operator=(const ShellCompositor&) = delete;

  void start();
  // Join the worker before the owner deletes related UI state / DIBs.
  void shutdown();

  // UI thread: move |frame| into pending and wake the worker.
  void commit(PaintCommit frame);

  // UI thread: after Commit, ask the worker to PostMessage
  // kShellPublishedMessage to |hwnd| once |generation| is published.
  // Later calls replace the target. Cleared on shutdown / after post.
  void notify_when_published(std::uint64_t generation, HWND hwnd);

  // UI thread / tests: block until |generation| is published or shutdown.
  // Product WM_PAINT must not call this (chrome hover stays non-blocking).
  bool wait_published(std::uint64_t generation);

  // UI thread: BitBlt the published front buffer into |hdc| for |dest|.
  // Blits first; only margins not covered by the front (or the whole rect when
  // empty) are filled with |fallback_fill| so NULL_BRUSH clients never show the
  // desktop on resize/move — without flashing shell_bg on every hover paint.
  // Returns the published generation that was blitted, or 0 if nothing was
  // drawn from the front buffer. Callers that fire OnShellPublished must use
  // this value (not a later published_generation() read) so notify cannot race
  // ahead of present.
  std::uint64_t present(HDC hdc,
                        const RECT& dest,
                        ui::gfx::Color fallback_fill);

  ui::gfx::ShellRaster shell_raster() const;
  std::uint64_t published_generation() const;

  // Drop worker DIBs (minimize / zero size). Safe on UI thread after shutdown
  // or while the worker is idle between frames; takes the mutex.
  void release_buffers();

 private:
  struct Dib {
    HDC dc = nullptr;
    HBITMAP dib = nullptr;
    HBITMAP old = nullptr;
    void* bits = nullptr;
    int w = 0;
    int h = 0;
  };

  void worker_main();
  void activate_pending();
  void raster_active();
  // Replay |frame| into |dc| for |dirty|. Selects |font| on every paint target
  // (including U4 temp DIBs — omitting that left TextOut on SYSTEM font so
  // full-frame chrome looked tiny until a small hover dirty reused |dc|).
  // |dc| must reference a top-down 32bpp DIB matching frame size.
  void raster_dirty_into(HDC dc,
                         int dib_w,
                         int dib_h,
                         const PaintCommit& frame,
                         Rect dirty,
                         bool full_frame,
                         HFONT font);
  HWND maybe_take_wake_hwnd_locked(std::uint64_t generation);
  bool ensure_dib(Dib* dib, int width_px, int height_px);
  void release_dib(Dib* dib);
  HFONT ensure_font_locked(int height_px);

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::thread worker_;
  bool stop_ = false;
  bool started_ = false;

  PaintCommit pending_;
  bool has_pending_ = false;
  PaintCommit active_;
  bool has_active_ = false;

  Dib buffers_[2];
  int front_ = 0;
  int back_ = 1;
  std::uint64_t published_gen_ = 0;

  HWND wake_hwnd_ = nullptr;
  std::uint64_t wake_gen_ = 0;
  std::uint64_t last_wake_posted_gen_ = 0;

  HFONT font_ = nullptr;
  int font_px_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_COMPOSITOR_SHELL_COMPOSITOR_H_
