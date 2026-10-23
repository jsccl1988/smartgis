// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// present walks one ordered effect list. Pass adapters live next to the
// passes they record.

#include "render/graph/frame_graph.h"

#include "base/core/log.h"

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
        // Slot names match EffectSlot order for Design B present triage.
        const char* slot_name = "unknown";
        switch (slot) {
          case EffectSlot::kBeforeOpaque:
            slot_name = "before_opaque";
            break;
          case EffectSlot::kOpaque:
            slot_name = "opaque";
            break;
          case EffectSlot::kAfterOpaque:
            slot_name = "after_opaque";
            break;
          case EffectSlot::kOverlay:
            slot_name = "overlay";
            break;
        }
        LOGGING(LOG_ERROR,
                "graph::present fail: record_effects slot=%s size=%ux%u "
                "effects=%zu",
                slot_name, in.width_px, in.height_px, in.effects.size());
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
    // Detail logged inside record_effects (slot=...).
    device->destroy_command_list(list);
    return false;
  }
  list->close();
  const bool ok = device->execute(list);
  device->destroy_command_list(list);
  if (!ok) {
    LOGGING(LOG_ERROR,
            "graph::present fail: device->execute size=%ux%u effects=%zu",
            in.width_px, in.height_px, in.effects.size());
    return false;
  }
  device->present();
  return true;
}

}  // namespace graph
}  // namespace render
