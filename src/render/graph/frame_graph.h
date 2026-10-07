// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// One view, one camera, one command list, one present. present walks Effect
// slots. Scene and map draws are Effect types outside this directory;
// ShellOverlayEffect (HUD src-over) is the graph-owned overlay peer.

#ifndef RENDER_GRAPH_FRAME_GRAPH_H_
#define RENDER_GRAPH_FRAME_GRAPH_H_

#include <cstdint>
#include <vector>

#include "render/render_export.h"
#include "render/rhi/rhi.h"

namespace render {
namespace graph {

// Record order for one present. kOpaque is terrain, models, and map world
// meshes. kOverlay is screen icons and text.
enum class EffectSlot {
  kBeforeOpaque,
  kOpaque,
  kAfterOpaque,
  kOverlay,
};

// Load policy and viewport for one Effect::record. The graph fills this;
// effects do not close, execute, or present.
struct RecordContext {
  rhi::Device* device = nullptr;
  rhi::CommandList* list = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
  const rhi::CameraMatrices* camera = nullptr;
  rhi::ColorLoadOp color_op = rhi::ColorLoadOp::kClear;
  bool shared_depth = false;
};

// Optional draw in one slot. The view holds non-owning pointers. Later
// domain simulations implement this; the slot does not name a domain.
class RENDER_EXPORT Effect {
 public:
  virtual ~Effect();

  virtual EffectSlot slot() const = 0;
  virtual bool clears_color() const { return false; }
  virtual bool uses_shared_depth() const { return false; }
  virtual bool record(const RecordContext& ctx) = 0;
};

// One viewport. |camera| is the only projection. Effects are non-owning.
// present records them in slot order, then in this vector's order inside a
// slot. A map host fills |camera| with make_ortho_camera of the view extent.
// A 3D host fills it with orbit matrices, or leaves it null on the Null
// backend so frustum cull stays off.
struct ViewInput {
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  const rhi::CameraMatrices* camera = nullptr;
  std::vector<Effect*> effects;
};

// Walks effects on one command list, then closes, executes, and presents.
// An empty effects vector clears once. Null device or zero size returns false.
RENDER_EXPORT bool present(rhi::Device* device, const ViewInput& in);

}  // namespace graph
}  // namespace render

#endif  // RENDER_GRAPH_FRAME_GRAPH_H_
