// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_PAINTER_H_
#define SCENIC_RHI2D_MAP_PAINTER_H_

#include <memory>
#include <mutex>

#include "gis/datasource/ogr/ogr_raster_layer.h"
#include "gis/tile/layer/provider_tile_layer.h"
#include "gis/map/map.h"
#include "gis/map/map_layer.h"
#include "scenic/render/err.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_pod.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "scenic/render/rhi2d/impl/common/surface/dib/owned.h"

class OGRFeature;
class OGRGeometry;
class OGRLayer;

namespace scenic {
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
               Rhi2dOwnedSurface* shared_front, base::Viewport* vir_vp1,
               base::Viewport* vir_vp2, std::mutex* shared_front_mu = nullptr);
  ~Rhi2dPainter();

  Rhi2dPainter(const Rhi2dPainter&) = delete;
  Rhi2dPainter& operator=(const Rhi2dPainter&) = delete;

  void set_scheduler(Rhi2dScheduler* sched);
  void set_layer_tree(Rhi2dLayerTreeImpl* tree);
  void set_context(RenderContext* rc);
  void set_render_options(const RenderOptions2d* options);

  int render_map(const gis::Map* map, int x, int y, int w, int h,
                 int op = R2_COPYPEN);
  int render_layer(const gis::MapLayer* layer, int op = R2_COPYPEN);
  int render_layer(OGRLayer* layer, int op = R2_COPYPEN);
  int render_layer(const gis::datasource::OgrRasterLayer* layer,
                   int op = R2_COPYPEN);
  int render_layer(const gis::tile::ProviderTileLayer* layer, int op = R2_COPYPEN);
  int render_feature(OGRFeature* feature, int op = R2_COPYPEN);
  int render_geometry(const OGRGeometry* geom, const Style* style,
                      int op = R2_COPYPEN);

 private:
  RenderContext& context();
  const RenderContext& context() const;
  void sync_carto_draw_links();

  Rhi2dCartoDraw* carto_draw_ = nullptr;
  Rhi2dOwnedSurface* back_buf_ = nullptr;
  Rhi2dOwnedSurface* shared_front_ = nullptr;
  base::Viewport* vir_vp1_ = nullptr;
  base::Viewport* vir_vp2_ = nullptr;
  std::mutex* shared_front_mu_ = nullptr;

  Rhi2dScheduler* scheduler_ = nullptr;
  Rhi2dLayerTreeImpl* layer_tree_ = nullptr;
  RenderContext* rc_ = nullptr;
  const RenderOptions2d* rd_options_ = nullptr;

  std::unique_ptr<GdiCartoFrame> carto2d_;
};

// Join process-wide tile/layer raster threads. Call from device Release after
// the FrameJob lane has exited — never from DllMain / FreeLibrary.
void rhi2d_shutdown_static_raster_runners();

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_MAP_PAINTER_H_
