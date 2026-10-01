// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_RHI2D_MAP_PAINTER_H_
#define SMT_LEGACY_RENDER_RHI2D_MAP_PAINTER_H_

#include <memory>
#include <mutex>

#include "gis/model/map/map.h"
#include "legacy/gis/present/carto/style.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "legacy/render/rhi2d/impl/common/surface/dib/owned.h"

class OGRFeature;
class OGRGeometry;
class OGRLayer;

using namespace base;
using namespace gis;

namespace render {

namespace detail {

class Rhi2dScheduler;
class Rhi2dLayerTreeImpl;

// Shared map/layer/feature/geometry paint orchestration. Draw primitives and
// style state live on Rhi2dCartoDraw; cancel/gen publish uses
// Rhi2dScheduler when set (worker FrameJob). LayerTreeHost owns the sole
// map-frame instance; Host may keep an overlay instance (scheduler null) for
// BeginRender RenderLayer* into the host encoder only.
class Rhi2dPainter {
 public:
  Rhi2dPainter(Rhi2dCartoDraw* carto_draw, Rhi2dOwnedSurface* back_buf,
                  Rhi2dOwnedSurface* shared_front, Viewport* vir_vp1,
                  Viewport* vir_vp2, std::mutex* shared_front_mu = nullptr);
  ~Rhi2dPainter();

  Rhi2dPainter(const Rhi2dPainter&) = delete;
  Rhi2dPainter& operator=(const Rhi2dPainter&) = delete;

  // Non-owning links set by the façade each frame / at construct.
  void set_scheduler(Rhi2dScheduler* sched);
  // Optional active tree for publish gating (gen / retire).
  void set_layer_tree(Rhi2dLayerTreeImpl* tree);
  void set_context(SmtRenderContext* rc);
  void set_render_options(const Smt2DRenderOptions* options);

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
  SmtRenderContext& context();
  const SmtRenderContext& context() const;
  void sync_carto_draw_links();

  Rhi2dCartoDraw* carto_draw_ = nullptr;
  Rhi2dOwnedSurface* back_buf_ = nullptr;
  Rhi2dOwnedSurface* shared_front_ = nullptr;
  Viewport* vir_vp1_ = nullptr;
  Viewport* vir_vp2_ = nullptr;
  std::mutex* shared_front_mu_ = nullptr;

  Rhi2dScheduler* scheduler_ = nullptr;
  Rhi2dLayerTreeImpl* layer_tree_ = nullptr;
  SmtRenderContext* rc_ = nullptr;
  const Smt2DRenderOptions* rd_options_ = nullptr;

  std::unique_ptr<GdiCartoFrame> carto2d_;
};

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_RHI2D_MAP_PAINTER_H_
