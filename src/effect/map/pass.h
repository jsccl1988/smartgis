// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_MAP_PASS_H_
#define EFFECT_MAP_PASS_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gis/vista/frame/frame.h"
#include "render/rhi/rhi.h"

namespace render {
namespace rhi {
class Buffer;
class CommandList;
class Device;
class Pipeline;
class Texture;
}  // namespace rhi

}  // namespace render

namespace effect {
namespace map {

// Wall time of the most recent Pass::record (place+upload+encode), milliseconds.
// Used by Map2d GPU present phase clocks; zero when record was not entered.
int64_t last_pass_record_ms();
void reset_last_pass_record_ms();

// CPU glyph coverage. Pass packs the codepoints a frame actually draws into
// one atlas; a failed rasterize skips that label and the frame continues.
class GlyphRasterizer {
 public:
  virtual ~GlyphRasterizer() = default;

  struct Bitmap {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
    float advance_px = 0.f;
  };

  virtual bool rasterize(uint32_t codepoint, float text_size_px,
                         Bitmap* out) = 0;
};

// GDI+ glyph rasterizer and advance metrics. No HWND. rasterize returns false
// when GDI+ startup or the face fails.
class WindowsGlyphRasterizer : public GlyphRasterizer,
                                              public gis::vista::GlyphMetrics {
 public:
  ~WindowsGlyphRasterizer() override = default;

  float advance_px(uint32_t codepoint, float text_size_px) const override;
  bool rasterize(uint32_t codepoint, float text_size_px, Bitmap* out) override;
};

// Records map meshes onto an existing RHI command list. The projection is
// the |camera| the host passed. MapScene passes make_ortho_camera of the
// view extent. A kPerspective View does not build a perspective matrix
// here; the host supplies that camera. A null camera binds the ortho of
// |view|'s extent. pixel_space icon and text are rotated about the anchor,
// then mapped into the view extent before the bind. Colors are 0xAARRGGBB.
class Pass {
 public:
  Pass();
  ~Pass();

  Pass(const Pass&) = delete;
  Pass& operator=(const Pass&) = delete;

  // |world_items| draws pixel_space == false. |overlay_items| draws icons
  // and text. Both default true (one shot, then close). A false pair member
  // leaves the list open. |color_op| kClear uses the frame background; a
  // world-only call with kLoad composites onto an earlier pass.
  bool record(
      render::rhi::Device* device, render::rhi::CommandList* list,
      const gis::vista::MapFrame& frame, const gis::vista::View& view,
      GlyphRasterizer* glyphs,
      const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                               int* w, int* h)>& load_raster,
      const std::function<bool(const std::string& symbol_id,
                               std::vector<uint8_t>* rgba, int* w, int* h)>&
          load_icon,
      const render::rhi::CameraMatrices* camera = nullptr,
      bool world_items = true, bool overlay_items = true,
      render::rhi::ColorLoadOp color_op = render::rhi::ColorLoadOp::kClear);

  // Programs created on the Device passed to record. Null until the first
  // record that reaches encode.
  render::rhi::Pipeline* solid_pipeline() const { return solid_pipeline_; }
  render::rhi::Pipeline* textured_pipeline() const { return textured_pipeline_; }

  // Drop cached uploads so the next record() re-places and re-uploads.
  // Call when MapFrame content changes (layout rebuild).
  void invalidate_uploaded();

 private:
  // Drop GPU ids without destroy_*. Callers may tear down Device first.
  void abandon();
  void release_uploaded();
  void destroy_pipelines();
  bool ensure_pipelines();

  render::rhi::Device* device_ = nullptr;
  render::rhi::Pipeline* solid_pipeline_ = nullptr;
  render::rhi::Pipeline* textured_pipeline_ = nullptr;
  std::vector<render::rhi::Buffer*> buffers_;
  std::vector<render::rhi::Texture*> textures_;
  // Opaque so pass.h does not pull detail::UploadedDraw into every TU that
  // only forward-declares Pass (unique_ptr delete needs a complete Pass only).
  struct DrawCache;
  std::unique_ptr<DrawCache> draw_cache_;
};

}  // namespace map
}  // namespace effect

#endif  // EFFECT_MAP_PASS_H_
