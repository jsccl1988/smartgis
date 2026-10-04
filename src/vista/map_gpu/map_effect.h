// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Adapts MapPass into one Effect slot. The frame graph does not include
// map types; hosts push this effect with the CPU MapIR they already built.

#ifndef VISTA_MAP_GPU_MAP_EFFECT_H_
#define VISTA_MAP_GPU_MAP_EFFECT_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "vista/map/ir.h"
#include "render/graph/frame_graph.h"

#include "vista/vista_export.h"

namespace vista {

class GlyphRasterizer;
class MapPass;

// Map meshes for one slot. kOpaque records world items. kOverlay records
// icons and text. record_all records both in one MapPass::record and reports
// slot kOpaque (map-only host). MapPass may close the list when both flags are
// true; present closes again. Stub CommandList::close only sets a flag.
class VISTA_EXPORT MapEffect final : public render::graph::Effect {
 public:
  MapEffect(render::graph::EffectSlot slot, MapPass* pass,
            const vista::MapIR* frame,
            const vista::View* view, GlyphRasterizer* glyphs,
            std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                               int* w, int* h)>
                load_raster,
            std::function<bool(const std::string& symbol_id,
                               std::vector<uint8_t>* rgba, int* w, int* h)>
                load_icon,
            bool record_all = false);

  render::graph::EffectSlot slot() const override;
  bool record(const render::graph::RecordContext& ctx) override;

 private:
  render::graph::EffectSlot slot_ = render::graph::EffectSlot::kOpaque;
  MapPass* pass_ = nullptr;
  const vista::MapIR* frame_ = nullptr;
  const vista::View* view_ = nullptr;
  GlyphRasterizer* glyphs_ = nullptr;
  std::function<bool(uint32_t, std::vector<uint8_t>*, int*, int*)> load_raster_;
  std::function<bool(const std::string&, std::vector<uint8_t>*, int*, int*)>
      load_icon_;
  bool record_all_ = false;
};

}  // namespace vista

#endif  // VISTA_MAP_GPU_MAP_EFFECT_H_
