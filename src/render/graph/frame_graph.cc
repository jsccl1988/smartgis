// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// present walks one ordered effect list. Pass adapters live next to the
// passes they record.

#include "render/graph/frame_graph.h"

namespace render {
namespace graph {
namespace {

constexpr EffectSlot kSlotOrder[] = {
    EffectSlot::kBeforeOpaque,
    EffectSlot::kOpaque,
    EffectSlot::kAfterOpaque,
    EffectSlot::kOverlay,
};

bool has_effect(const ViewInput& in) {
  for (Effect* effect : in.effects) {
    if (effect) {
      return true;
    }
  }
  return false;
}

void clear_once(rhi::CommandList* list, uint32_t width, uint32_t height) {
  rhi::RenderPassDesc desc;
  desc.width = width;
  desc.height = height;
  desc.load_op = rhi::ColorLoadOp::kClear;
  list->begin_render_pass(desc);
  list->end_render_pass();
}

bool record_effects(rhi::Device* device, rhi::CommandList* list,
                    const ViewInput& in) {
  rhi::ColorLoadOp color_op = rhi::ColorLoadOp::kClear;
  bool shared_depth = false;
  for (EffectSlot slot : kSlotOrder) {
    for (Effect* effect : in.effects) {
      if (!effect || effect->slot() != slot) {
        continue;
      }
      RecordContext ctx;
      ctx.device = device;
      ctx.list = list;
      ctx.width = in.width_px;
      ctx.height = in.height_px;
      ctx.camera = in.camera;
      ctx.color_op = color_op;
      ctx.shared_depth = shared_depth;
      if (!effect->record(ctx)) {
        return false;
      }
      if (effect->clears_color()) {
        color_op = rhi::ColorLoadOp::kLoad;
      }
      if (effect->uses_shared_depth()) {
        shared_depth = true;
      }
    }
  }
  return true;
}

}  // namespace

Effect::~Effect() = default;

bool present(rhi::Device* device, const ViewInput& in) {
  if (!device || in.width_px == 0 || in.height_px == 0) {
    return false;
  }
  rhi::CommandList* list = device->create_command_list();
  if (!list) {
    return false;
  }
  bool recorded = true;
  if (has_effect(in)) {
    recorded = record_effects(device, list, in);
  } else {
    clear_once(list, in.width_px, in.height_px);
  }
  if (!recorded) {
    device->destroy_command_list(list);
    return false;
  }
  list->close();
  const bool ok = device->execute(list);
  device->destroy_command_list(list);
  if (ok) {
    device->present();
  }
  return ok;
}

}  // namespace graph
}  // namespace render
