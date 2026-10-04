// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_MAP_GPU_PASS_H_
#define VISTA_MAP_GPU_PASS_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "vista/map/ir.h"
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

#include "vista/vista_export.h"

namespace vista {

// Wall time of the most recent MapPass::record (place+upload+encode), milliseconds.
// Used by Map2d GPU present phase clocks; zero when record was not entered.
VISTA_EXPORT int64_t last_pass_record_ms();
VISTA_EXPORT void reset_last_pass_record_ms();

// CPU glyph coverage. MapPass packs the codepoints a frame actually draws into
// one atlas; a failed rasterize skips that label and the frame continues.
class VISTA_EXPORT GlyphRasterizer {
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
class VISTA_EXPORT WindowsGlyphRasterizer : public GlyphRasterizer,
                                              public vista::GlyphMetrics {
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
class VISTA_EXPORT MapPass {
 public:
  MapPass();
  ~MapPass();

  MapPass(const MapPass&) = delete;
  MapPass& operator=(const MapPass&) = delete;

  // |world_items| draws pixel_space == false. |overlay_items| draws icons
  // and text. Both default true (one shot, then close). A false pair member
  // leaves the list open. |color_op| kClear uses the frame background; a
  // world-only call with kLoad composites onto an earlier pass.
  bool record(
      render::rhi::Device* device, render::rhi::CommandList* list,
      const vista::MapIR& frame, const vista::View& view,
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
  // Textured program compiled with BlendMode::kMultiply for hillshade.
  render::rhi::Pipeline* multiply_pipeline() const { return multiply_pipeline_; }

  // Drop cached uploads so the next record() re-places and re-uploads.
  // Call when MapIR content changes (layout rebuild).
  void invalidate_uploaded();

  // kReuseIfCached: camera-only encode when DrawCache is warm.
  // kIncremental: reuse DrawCache when world+overlay identity hashes match;
  // otherwise one Display-thread place+upload (same Buffer list as kReplace).
  // kReplace: never reuse.
  enum class UploadPolicy {
    kReuseIfCached,
    kIncremental,
    kReplace,
  };
  void set_upload_policy(UploadPolicy policy) { upload_policy_ = policy; }

 private:
  // Drop GPU ids without destroy_*. Callers may tear down Device first.
  void abandon();
  void release_uploaded();
  void destroy_pipelines();
  bool ensure_pipelines();

  render::rhi::Device* device_ = nullptr;
  render::rhi::Pipeline* solid_pipeline_ = nullptr;
  render::rhi::Pipeline* textured_pipeline_ = nullptr;
  render::rhi::Pipeline* multiply_pipeline_ = nullptr;
  std::vector<render::rhi::Buffer*> buffers_;
  std::vector<render::rhi::Texture*> textures_;
  // Opaque so pass.h does not pull detail::UploadedDraw into every TU that
  // only forward-declares MapPass (unique_ptr delete needs a complete type).
  struct DrawCache;
  std::unique_ptr<DrawCache> draw_cache_;
  UploadPolicy upload_policy_ = UploadPolicy::kReuseIfCached;

  // Device-thread upload of cache_key misses. Hits keep their buffers.
  bool upload_keyed_slices(
      render::rhi::CommandList* list, const vista::MapIR& frame,
      const vista::View& view, GlyphRasterizer* glyphs,
      const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                               int* w, int* h)>& load_raster,
      const std::function<bool(const std::string& symbol_id,
                               std::vector<uint8_t>* rgba, int* w, int* h)>&
          load_icon,
      const render::rhi::CameraMatrices* camera, bool world_items,
      bool overlay_items, render::rhi::ColorLoadOp color_op,
      uint64_t world_hash, uint64_t overlay_hash);
};

}  // namespace vista

#endif  // VISTA_MAP_GPU_PASS_H_
