// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_SUBMIT_H_
#define SCENIC_RHI2D_MAP_SUBMIT_H_

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#include "scenic/render/rhi2d/impl/common/cc/raster_tile.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/encode/command_encoder.h"
#include "scenic/render/rhi2d/impl/common/surface/dib/owned.h"

namespace scenic {
namespace detail {

class Rhi2dLayerTreeImpl;
class Rhi2dScheduler;

// Records one map pass; in layer-parallel mode seals one CommandBuffer per GIS
// layer. HDC play stays in execute_map_pass.
class MapEncodePass {
 public:
  MapEncodePass(Rhi2dCartoDraw* carto, Rhi2dSurface* target,
                bool layer_parallel);

  void begin(COLORREF clear_if_serial);
  void seal_layer();
  void mark_cmds();
  void end_record();

  bool layer_parallel() const { return layer_parallel_; }
  std::vector<Rhi2dCommandBuffer>& layer_bufs() { return layer_bufs_; }
  Rhi2dCommandEncoder& encoder() { return encoder_; }

 private:
  Rhi2dCartoDraw* carto_ = nullptr;
  Rhi2dSurface* target_ = nullptr;
  bool layer_parallel_ = false;
  bool has_cmds_ = false;
  Rhi2dCommandEncoder encoder_;
  std::vector<Rhi2dCommandBuffer> layer_bufs_;
};

struct MapExecuteArgs {
  Rhi2dOwnedSurface* back = nullptr;
  Rhi2dLayerTreeImpl* tree = nullptr;
  Rhi2dParallelMode mode = Rhi2dParallelMode::kSerial;
  int w = 0;
  int h = 0;
  uint64_t job_gen = 0;
  bool have_map = false;
  std::function<bool()> aborted;
};

// Serial / tile / layer execute into |args.back|. False means the FrameJob
// was aborted (caller returns kErrNone, skip publish).
bool execute_map_pass(MapEncodePass* pass, const MapExecuteArgs& args);

void draw_parallel_strategy_label(Rhi2dOwnedSurface* back,
                                  Rhi2dParallelMode mode);

struct MapPublishArgs {
  Rhi2dOwnedSurface* back = nullptr;
  Rhi2dOwnedSurface* shared_front = nullptr;
  std::mutex* shared_front_mu = nullptr;
  Rhi2dScheduler* scheduler = nullptr;
  Rhi2dLayerTreeImpl* tree = nullptr;
  Viewport* vir_vp1 = nullptr;
  Viewport* vir_vp2 = nullptr;
  const Viewport* src_vp = nullptr;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  uint64_t job_gen = 0;
  bool have_map = false;
  std::function<bool()> aborted;
};

void publish_map_front(const MapPublishArgs& args);

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_MAP_SUBMIT_H_
