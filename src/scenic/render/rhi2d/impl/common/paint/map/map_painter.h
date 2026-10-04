// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_PAINTER_H_
#define SCENIC_RHI2D_MAP_PAINTER_H_

#include <memory>
#include <mutex>

#include "gis/datasource/ogr/ogr_raster_layer.h"
#include "gis/carto/tile/provider_tile_layer.h"
#include "gis/map/map.h"
#include "gis/map/map_layer.h"
#include "scenic/detail/style.h"
#include "scenic/detail/err.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "scenic/render/rhi2d/impl/common/surface/dib/owned.h"

class OGRFeature;
class OGRGeometry;
class OGRLayer;

using namespace base;
using namespace gis;

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
               Rhi2dOwnedSurface* shared_front, Viewport* vir_vp1,
               Viewport* vir_vp2, std::mutex* shared_front_mu = nullptr);
  ~Rhi2dPainter();

  Rhi2dPainter(const Rhi2dPainter&) = delete;
  Rhi2dPainter& operator=(const Rhi2dPainter&) = delete;

  void set_scheduler(Rhi2dScheduler* sched);
  void set_layer_tree(Rhi2dLayerTreeImpl* tree);
  void set_context(RenderContext* rc);
  void set_render_options(const RenderOptions2d* options);

  int render_map(const Map* map, int x, int y, int w, int h,
                 int op = R2_COPYPEN);
  int render_layer(const MapLayer* layer, int op = R2_COPYPEN);
  int render_layer(OGRLayer* layer, int op = R2_COPYPEN);
  int render_layer(const datasource::OgrRasterLayer* layer,
                   int op = R2_COPYPEN);
  int render_layer(const tile::ProviderTileLayer* layer, int op = R2_COPYPEN);
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
  Viewport* vir_vp1_ = nullptr;
  Viewport* vir_vp2_ = nullptr;
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
