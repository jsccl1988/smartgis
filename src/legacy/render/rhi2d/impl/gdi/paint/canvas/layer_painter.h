// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_PAINT_LAYER_PAINTER_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_PAINT_LAYER_PAINTER_H_

#include <memory>
#include <mutex>

#include "gis/model/map/map.h"
#include "legacy/gis/present/carto/style.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/paint_canvas.h"
#include "legacy/render/rhi2d/impl/gdi/surface/surface_pool.h"

class OGRFeature;
class OGRGeometry;
class OGRLayer;

using namespace base;
using namespace gis;

namespace render {

namespace detail {

class GdiRasterScheduler;

// Shared map/layer/feature/geometry paint orchestration. Draw primitives and
// style state live on GdiPaintCanvas; cancel/gen publish uses
// GdiRasterScheduler when set (worker FrameJob). Host sync paint leaves
// scheduler null.
class GdiLayerPainter {
 public:
  GdiLayerPainter(GdiPaintCanvas* canvas, GdiOwnedSurface* back_buf,
                  GdiOwnedSurface* shared_front, Viewport* vir_vp1,
                  Viewport* vir_vp2, std::mutex* shared_front_mu = nullptr);
  ~GdiLayerPainter();

  GdiLayerPainter(const GdiLayerPainter&) = delete;
  GdiLayerPainter& operator=(const GdiLayerPainter&) = delete;

  // Non-owning links set by the façade each frame / at construct.
  void set_scheduler(GdiRasterScheduler* sched);
  void set_context(SmtRenderContex* rc);
  void set_render_pra(const Smt2DRenderPra* pra);

  int render_map(const SmtMap* map, int x, int y, int w, int h,
                 int op = R2_COPYPEN);
  int render_layer(const SmtLayer* layer, int op = R2_COPYPEN);
  int render_layer(OGRLayer* layer, int op = R2_COPYPEN);
  int render_layer(const SmtRasterLayer* layer, int op = R2_COPYPEN);
  int render_layer(const SmtTileLayer* layer, int op = R2_COPYPEN);
  int render_feature(OGRFeature* feature, int op = R2_COPYPEN);
  int render_geometry(const OGRGeometry* geom, const SmtStyle* style,
                      int op = R2_COPYPEN);

 private:
  SmtRenderContex& context();
  const SmtRenderContex& context() const;
  void sync_canvas_links();

  GdiPaintCanvas* canvas_ = nullptr;
  GdiOwnedSurface* back_buf_ = nullptr;
  GdiOwnedSurface* shared_front_ = nullptr;
  Viewport* vir_vp1_ = nullptr;
  Viewport* vir_vp2_ = nullptr;
  std::mutex* shared_front_mu_ = nullptr;

  GdiRasterScheduler* scheduler_ = nullptr;
  SmtRenderContex* rc_ = nullptr;
  const Smt2DRenderPra* rd_pra_ = nullptr;

  std::unique_ptr<GdiCartoFrame> carto2d_;
};

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_PAINT_LAYER_PAINTER_H_
