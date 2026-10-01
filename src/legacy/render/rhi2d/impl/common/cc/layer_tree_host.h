// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_LAYER_TREE_HOST_H_
#define LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_LAYER_TREE_HOST_H_

#include <cstdint>
#include <memory>
#include <mutex>

#include <windows.h>

#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/render/rhi2d/impl/common/cc/layer_tree_impl.h"
#include "legacy/render/rhi2d/impl/common/cc/scheduler.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/frame/context.h"
#include "legacy/render/rhi2d/impl/common/surface/dib/owned.h"

namespace render {
namespace detail {

class Rhi2dPainter;
class Rhi2dCartoDraw;

// Single leftover map2d frame owner (Chromium LayerTreeHost analogue):
// commit / schedule / activate / draw, plus private back buffer + painter.
// Present stays on PresentController; IR submit stays on submit_surface.
class Rhi2dLayerTreeHost {
 public:
  Rhi2dLayerTreeHost(HINSTANCE h_inst, Viewport& vir_viewport1,
                     Viewport& vir_viewport2, std::mutex* shared_front_mu);
  ~Rhi2dLayerTreeHost();

  Rhi2dLayerTreeHost(const Rhi2dLayerTreeHost&) = delete;
  Rhi2dLayerTreeHost& operator=(const Rhi2dLayerTreeHost&) = delete;

  int init(HWND hwnd, const char* logname);
  int resize(int orgx, int orgy, int cx, int cy, Rhi2dOwnedSurface& front);

  // Commit pending context (HWND). Coalesces while the worker is busy.
  int stage_frame(const SmtRenderContext& ctx,
                  const Smt2DRenderOptions& options);
  void commit(const SmtRenderContext& rc);
  void commit(const SmtRenderContext& rc, const RECT& damage,
              bool full_damage = false);

  // Schedule one draw when idle. Returns false if busy.
  bool submit_frame(const SmtRenderContext& ctx,
                    const Smt2DRenderOptions& options);
  void schedule();

  // HWND-thread sync map paint (back → shared front). Sole frame painter.
  // Requires !is_busy(); restores worker painter links before return.
  int paint_map_sync(const SmtRenderContext& ctx,
                     const Smt2DRenderOptions& options);

  void cancel();
  bool wait_idle(int timeout_ms);
  bool shutdown();

  bool is_busy() const;
  bool has_pending() const;
  bool has_exited() const;

  uint64_t job_generation() const;
  uint64_t published_generation() const;
  void mark_published(uint64_t gen);
  bool should_abort(uint64_t paint_job_gen) const;
  bool can_publish(uint64_t paint_job_gen) const;

  SmtRenderContext& context();
  const SmtRenderContext& context() const;

  Rhi2dScheduler& scheduler() { return scheduler_; }
  const Rhi2dScheduler& scheduler() const { return scheduler_; }

  Rhi2dLayerTreeImpl& tree() { return tree_; }
  const Rhi2dLayerTreeImpl& tree() const { return tree_; }

 private:
  void on_worker_tick();
  void apply_pending_damage();
  void paint_once();
  void bind_painter();

  HINSTANCE h_inst_;
  HWND hwnd_ = nullptr;
  Viewport& vir_viewport1_;
  Viewport& vir_viewport2_;

  // Paint target owned here. |shared_buf_| is share_from(host map_front_) —
  // Host owns the published HBITMAP; this is a non-owning alias after resize.
  Rhi2dOwnedSurface back_buf_;
  Rhi2dOwnedSurface shared_buf_;
  Smt2DRenderOptions rd_options_{};

  Rhi2dScheduler scheduler_;
  Rhi2dLayerTreeImpl tree_;
  RECT pending_damage_{0, 0, 0, 0};
  bool pending_full_damage_ = true;

  std::unique_ptr<Rhi2dCartoDraw> carto_draw_;
  std::unique_ptr<Rhi2dPainter> painter_;
};

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_LAYER_TREE_HOST_H_
