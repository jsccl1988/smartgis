// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/map/detail/encode.h"

#include "effect/map/detail/color.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace map {
namespace detail {
namespace {

void draw_solid(render::rhi::CommandList* list, render::rhi::Pipeline* solid,
                render::rhi::Buffer* vb, render::rhi::Buffer* ib,
                uint32_t index_count, uint32_t stride, float r, float g,
                float b, float a) {
  list->bind_texture(nullptr, render::programs::kTextureSlot);
  list->set_pipeline(solid);
  list->set_blend_mode(a < 0.999f ? render::rhi::BlendMode::kSrcAlpha
                                  : render::rhi::BlendMode::kOpaque);
  const render::programs::Color color{r, g, b, a};
  list->set_constants(render::programs::kColorSlot, &color,
                      static_cast<uint32_t>(sizeof(color)));
  list->bind_vertex_buffer(vb, 0, stride);
  list->bind_index_buffer(ib, 0);
  list->draw_indexed(index_count, 1, 0, 0, 0);
}

void draw_textured(render::rhi::CommandList* list,
                   render::rhi::Pipeline* textured, render::rhi::Buffer* vb,
                   render::rhi::Buffer* ib, uint32_t index_count,
                   uint32_t stride, render::rhi::Texture* texture, float r,
                   float g, float b, float a) {
  list->set_pipeline(textured);
  list->bind_texture(texture, render::programs::kTextureSlot);
  list->set_blend_mode(render::rhi::BlendMode::kSrcAlpha);
  const render::programs::Color tint{r, g, b, a};
  list->set_constants(render::programs::kColorSlot, &tint,
                      static_cast<uint32_t>(sizeof(tint)));
  list->bind_vertex_buffer(vb, 0, stride);
  list->bind_index_buffer(ib, 0);
  list->draw_indexed(index_count, 1, 0, 0, 0);
}

}  // namespace

void encode_draws(render::rhi::CommandList* list, const gis::vista::View& view,
                  const gis::vista::MapFrame& frame,
                  const std::vector<UploadedDraw>& draws,
                  const render::rhi::CameraMatrices* camera,
                  render::rhi::ColorLoadOp load_op, bool close_list,
                  render::rhi::Pipeline* solid,
                  render::rhi::Pipeline* textured) {
  if (!list) {
    return;
  }
  float clear_r = 0.f;
  float clear_g = 0.f;
  float clear_b = 0.f;
  float clear_a = 1.f;
  unpack_rgba(frame.background_rgba, frame.background_opacity, &clear_r,
              &clear_g, &clear_b, &clear_a);

  if (camera) {
    list->bind_camera(*camera);
  } else {
    list->bind_camera(render::rhi::make_ortho_camera(
        static_cast<float>(view.min_x), static_cast<float>(view.max_x),
        static_cast<float>(view.min_y), static_cast<float>(view.max_y), -1.f,
        1.f));
  }

  render::rhi::RenderPassDesc desc;
  desc.clear_r = clear_r;
  desc.clear_g = clear_g;
  desc.clear_b = clear_b;
  desc.clear_a = clear_a;
  desc.width = view.width_px;
  desc.height = view.height_px;
  desc.load_op = load_op;
  desc.enable_depth = false;
  list->begin_render_pass(desc);
  list->set_viewport(0.f, 0.f, static_cast<float>(view.width_px),
                     static_cast<float>(view.height_px), 0.f, 1.f);
  list->set_depth_mode(render::rhi::DepthMode::kDisabled);

  for (const UploadedDraw& draw : draws) {
    if (draw.textured) {
      draw_textured(list, textured, draw.vertices, draw.indices,
                    draw.index_count, draw.stride, draw.texture, draw.r,
                    draw.g, draw.b, draw.a);
    } else {
      draw_solid(list, solid, draw.vertices, draw.indices, draw.index_count,
                 draw.stride, draw.r, draw.g, draw.b, draw.a);
    }
  }

  list->end_render_pass();
  if (close_list) {
    list->close();
  }
}

}  // namespace detail
}  // namespace map
}  // namespace effect
