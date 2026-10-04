// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/detail/encode.h"

#include "vista/map/detail/color.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"

namespace vista {
namespace detail {
namespace {

void draw_solid(render::rhi::CommandList* list, render::rhi::Pipeline* solid,
                render::rhi::Buffer* vb, render::rhi::Buffer* ib,
                uint32_t index_count, uint32_t first_index, uint32_t stride,
                float r, float g, float b, float a) {
  list->bind_texture(nullptr, render::programs::kTextureSlot);
  list->set_pipeline(solid);
  list->set_blend_mode(a < 0.999f ? render::rhi::BlendMode::kSrcAlpha
                                  : render::rhi::BlendMode::kOpaque);
  const render::programs::Color color{r, g, b, a};
  list->set_constants(render::programs::kColorSlot, &color,
                      static_cast<uint32_t>(sizeof(color)));
  list->bind_vertex_buffer(vb, 0, stride);
  list->bind_index_buffer(ib, 0);
  list->draw_indexed(index_count, 1, first_index, 0, 0);
}

void draw_textured(render::rhi::CommandList* list,
                   render::rhi::Pipeline* textured, render::rhi::Pipeline* multiply,
                   render::rhi::Buffer* vb, render::rhi::Buffer* ib,
                   uint32_t index_count, uint32_t first_index, uint32_t stride,
                   render::rhi::Texture* texture, float r, float g, float b,
                   float a, render::rhi::BlendMode blend) {
  const bool shade = blend == render::rhi::BlendMode::kMultiply;
  list->set_pipeline(shade && multiply != nullptr ? multiply : textured);
  list->bind_texture(texture, render::programs::kTextureSlot);
  list->set_blend_mode(shade ? render::rhi::BlendMode::kMultiply
                             : render::rhi::BlendMode::kSrcAlpha);
  const render::programs::Color tint{r, g, b, a};
  list->set_constants(render::programs::kColorSlot, &tint,
                      static_cast<uint32_t>(sizeof(tint)));
  list->bind_vertex_buffer(vb, 0, stride);
  list->bind_index_buffer(ib, 0);
  list->draw_indexed(index_count, 1, first_index, 0, 0);
}

}  // namespace

void encode_draws(render::rhi::CommandList* list, const vista::View& view,
                  const vista::MapFrame& frame,
                  const std::vector<UploadedDraw>& draws,
                  const render::rhi::CameraMatrices* camera,
                  render::rhi::ColorLoadOp load_op, bool close_list,
                  render::rhi::Pipeline* solid,
                  render::rhi::Pipeline* textured,
                  render::rhi::Pipeline* multiply) {
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
      draw_textured(list, textured, multiply, draw.vertices, draw.indices,
                    draw.index_count, draw.first_index, draw.stride,
                    draw.texture, draw.r, draw.g, draw.b, draw.a, draw.blend);
    } else {
      draw_solid(list, solid, draw.vertices, draw.indices, draw.index_count,
                 draw.first_index, draw.stride, draw.r, draw.g, draw.b,
                 draw.a);
    }
  }

  list->end_render_pass();
  if (close_list) {
    list->close();
  }
}

}  // namespace detail
}  // namespace vista
