// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Staged shell overlay BGRA for DrawRequest.shell / GPU present fold-in.

#include "ui/views/map/viewport/draw_host.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/map/viewport/detail/overlay_hole_keys.h"

namespace ui {
namespace views {

void DrawHost::clear_shell_overlay() {
  std::lock_guard<std::mutex> lock(shell_mu_);
  shell_bgra_.clear();
  shell_width_px_ = 0;
  shell_height_px_ = 0;
  shell_stride_bytes_ = 0;
  shell_generation_.fetch_add(1, std::memory_order_acq_rel);
}

void DrawHost::commit_shell_overlay(const uint8_t* bgra, uint32_t width_px,
                                    uint32_t height_px, uint32_t stride_bytes,
                                    uint64_t generation,
                                    uint32_t hole_clear_argb,
                                    uint32_t hole_clear_argb_alt) {
  // Copy immediately; ShellRaster pointers must not outlive the caller.
  // GPU present folds this into DrawRequest.shell via snapshot + overlay.
  if (!bgra || width_px == 0 || height_px == 0) {
    return;
  }
  const uint32_t stride =
      stride_bytes != 0 ? stride_bytes : width_px * 4u;
  if (stride < width_px * 4u) {
    return;
  }
  bool changed = false;
  LARGE_INTEGER t0 = {};
  QueryPerformanceCounter(&t0);
  {
    std::lock_guard<std::mutex> lock(shell_mu_);
    if (generation != 0 &&
        generation == shell_generation_.load(std::memory_order_relaxed) &&
        shell_width_px_ == width_px && shell_height_px_ == height_px &&
        !shell_bgra_.empty()) {
      return;
    }
    shell_bgra_.resize(static_cast<size_t>(width_px) * height_px * 4u);
    for (uint32_t y = 0; y < height_px; ++y) {
      std::memcpy(shell_bgra_.data() + static_cast<size_t>(y) * width_px * 4u,
                  bgra + static_cast<size_t>(y) * stride,
                  static_cast<size_t>(width_px) * 4u);
    }
    ui::gfx::note_overlay_copy_bytes(
        static_cast<std::uint64_t>(width_px) * height_px * 4u);
    // Native draw HWNDs are skipped in shell paint; parents still bleed opaque
    // panel/shell fills into the HWND rect. Src-over of those fills on GPU
    // present briefly shows a correct GPU map then covers it. Zero alpha for
    // Theme horizon fills that land in the map crop; keep real HUD pixels.
    uint32_t hole_bgr[detail::kOverlayHoleKeyCap] = {};
    const int hole_n = detail::collect_overlay_hole_bgr_keys(
        hole_clear_argb, hole_clear_argb_alt, hole_bgr);
    uint8_t* px = shell_bgra_.data();
    const size_t n = static_cast<size_t>(width_px) * height_px;
    for (size_t i = 0; i < n; ++i, px += 4) {
      // DIB is BGRA; pack as 0x00RRGGBB to match color_rgb / Theme ARGB.
      const uint32_t key = (static_cast<uint32_t>(px[2]) << 16) |
                           (static_cast<uint32_t>(px[1]) << 8) |
                           static_cast<uint32_t>(px[0]);
      for (int h = 0; h < hole_n; ++h) {
        if (hole_bgr[h] == key) {
          px[3] = 0;
          break;
        }
      }
    }
    shell_width_px_ = width_px;
    shell_height_px_ = height_px;
    shell_stride_bytes_ = width_px * 4u;
    if (generation == 0) {
      shell_generation_.fetch_add(1, std::memory_order_acq_rel);
    } else {
      shell_generation_.store(generation, std::memory_order_release);
    }
    changed = true;
  }
  if (changed) {
    LARGE_INTEGER t1 = {};
    QueryPerformanceCounter(&t1);
    if (t1.QuadPart > t0.QuadPart) {
      ui::gfx::note_overlay_commit_qpc(
          static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
    }
    // Map-region shell changed: wake BeginFrame so HUD lands in the next GPU
    // present. Shell-only dirty must be filtered by the caller (BrowserView).
    request_frame();
  }
}

bool DrawHost::snapshot_shell_overlay(std::vector<uint8_t>* out_bgra,
                                      ui::gfx::ShellRaster* out_shell,
                                      uint64_t* out_generation) const {
  if (!out_bgra || !out_shell) {
    return false;
  }
  std::lock_guard<std::mutex> lock(shell_mu_);
  if (shell_bgra_.empty() || shell_width_px_ == 0 || shell_height_px_ == 0) {
    *out_shell = {};
    if (out_generation) {
      *out_generation = 0;
    }
    return false;
  }
  *out_bgra = shell_bgra_;
  out_shell->bgra = out_bgra->data();
  out_shell->width_px = shell_width_px_;
  out_shell->height_px = shell_height_px_;
  out_shell->stride_bytes = shell_stride_bytes_ != 0
                                ? shell_stride_bytes_
                                : shell_width_px_ * 4u;
  if (out_generation) {
    *out_generation = shell_generation_.load(std::memory_order_acquire);
  }
  return true;
}

}  // namespace views
}  // namespace ui
