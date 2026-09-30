// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GDI_RENDERTHREAD_H
#define _GDI_RENDERTHREAD_H

#include <cstdint>
#include <memory>
#include <mutex>

#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/paint_context.h"
#include "legacy/render/rhi2d/impl/gdi/surface/surface_pool.h"

namespace render {
namespace detail {
class GdiRasterScheduler;
class GdiLayerPainter;
class GdiPaintCanvas;
}  // namespace detail

// Thin leftover FrameJob façade: schedules one GDI map paint off the HWND
// thread via composed raster scheduler / layer painter / paint canvas.
class SmtGdiRenderThread {
 public:
  SmtGdiRenderThread(HINSTANCE h_inst, Viewport& vir_viewport1,
                     Viewport& vir_viewport2, std::mutex* shared_front_mu);
  ~SmtGdiRenderThread();

  SmtGdiRenderThread(const SmtGdiRenderThread&) = delete;
  SmtGdiRenderThread& operator=(const SmtGdiRenderThread&) = delete;

  // Init HWND + start the serial executor (suspended until submit_frame).
  int init(HWND hwnd, const char* logname);
  // Resize private back buffer and share the device front.
  int resize(int orgx, int orgy, int cx, int cy, GdiOwnedSurface& front);
  // Stage latest job (coalesce + cancel in-flight if busy). Does not start
  // paint.
  int stage_frame(const SmtRenderContex& ctx, const Smt2DRenderPra& pra);
  // Apply context when idle and submit one FrameJob. Returns false if busy.
  bool submit_frame(const SmtRenderContex& ctx, const Smt2DRenderPra& pra);

  void cancel();
  bool wait_idle(int timeout_ms);
  // True if published_generation advanced past baseline; updates *baseline.
  bool try_present(uint64_t* baseline);
  // Returns true if the worker was detached (HWND-thread stop path).
  bool shutdown();

  bool is_busy() const;
  bool has_pending() const;
  bool has_exited() const;
  uint64_t job_generation() const;
  uint64_t published_generation() const;

 private:
  void paint_once();

  HINSTANCE h_inst_;
  HWND hwnd_ = nullptr;
  Viewport& vir_viewport1_;
  Viewport& vir_viewport2_;

  GdiOwnedSurface back_buf_;
  GdiOwnedSurface shared_buf_;
  Smt2DRenderPra rd_pra_{};

  std::unique_ptr<detail::GdiRasterScheduler> scheduler_;
  std::unique_ptr<detail::GdiPaintCanvas> canvas_;
  std::unique_ptr<detail::GdiLayerPainter> painter_;
};

}  // namespace render

#endif  // _GDI_RENDERTHREAD_H
