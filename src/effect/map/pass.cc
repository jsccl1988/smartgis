// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/map/pass.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

#include "base/memory/arena.h"
#include "base/trace/event/process_trace.h"
#include "effect/map/detail/atlas.h"
#include "effect/map/detail/encode.h"
#include "effect/map/detail/place.h"
#include "effect/map/detail/upload.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"

namespace effect {
namespace map {
namespace {

std::atomic<int64_t> g_last_pass_record_ms{0};

}  // namespace

int64_t last_pass_record_ms() {
  return g_last_pass_record_ms.load(std::memory_order_relaxed);
}

void reset_last_pass_record_ms() {
  g_last_pass_record_ms.store(0, std::memory_order_relaxed);
}

struct Pass::DrawCache {
  std::vector<detail::UploadedDraw> world_draws;
  std::vector<detail::UploadedDraw> full_draws;
};

Pass::Pass() : draw_cache_(std::make_unique<DrawCache>()) {}

Pass::~Pass() { abandon(); }

void Pass::destroy_pipelines() {
  if (device_) {
    device_->destroy_pipeline(solid_pipeline_);
    device_->destroy_pipeline(textured_pipeline_);
  }
  solid_pipeline_ = nullptr;
  textured_pipeline_ = nullptr;
}

bool Pass::ensure_pipelines() {
  if (!device_) {
    return false;
  }
  if (!solid_pipeline_) {
    solid_pipeline_ =
        device_->create_graphics_pipeline(render::programs::solid_pipeline_desc());
  }
  if (!textured_pipeline_) {
    textured_pipeline_ = device_->create_graphics_pipeline(
        render::programs::textured_pipeline_desc());
  }
  return solid_pipeline_ != nullptr && textured_pipeline_ != nullptr;
}

void Pass::abandon() {
  solid_pipeline_ = nullptr;
  textured_pipeline_ = nullptr;
  buffers_.clear();
  textures_.clear();
  if (draw_cache_) {
    draw_cache_->world_draws.clear();
    draw_cache_->full_draws.clear();
  }
  device_ = nullptr;
}

void Pass::release_uploaded() {
  if (device_) {
    for (render::rhi::Buffer* buffer : buffers_) {
      device_->destroy_buffer(buffer);
    }
    for (render::rhi::Texture* texture : textures_) {
      device_->destroy_texture(texture);
    }
  }
  buffers_.clear();
  textures_.clear();
  if (draw_cache_) {
    draw_cache_->world_draws.clear();
    draw_cache_->full_draws.clear();
  }
}

void Pass::invalidate_uploaded() { release_uploaded(); }

bool Pass::record(
    render::rhi::Device* device, render::rhi::CommandList* list,
    const gis::vista::MapFrame& frame, const gis::vista::View& view,
    GlyphRasterizer* glyphs,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster,
    const std::function<bool(const std::string& symbol_id,
                             std::vector<uint8_t>* rgba, int* w, int* h)>&
        load_icon,
    const render::rhi::CameraMatrices* camera, bool world_items,
    bool overlay_items, render::rhi::ColorLoadOp color_op) {
  struct RecordClock {
    std::chrono::steady_clock::time_point t0 =
        std::chrono::steady_clock::now();
    ~RecordClock() {
      g_last_pass_record_ms.store(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - t0)
              .count(),
          std::memory_order_relaxed);
    }
  } record_clock;
  if (!device || !list || view.width_px == 0 || view.height_px == 0) {
    return false;
  }
  if (!world_items && !overlay_items) {
    return true;
  }
  if (!draw_cache_) {
    draw_cache_ = std::make_unique<DrawCache>();
  }
  // Fresh TLS scratch for place/upload temps this record.
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(256 * 1024);
  }
  BASE_TRACE_EVENT("upload", "map2d.upload");
  if (device_ != nullptr && device_ != device) {
    destroy_pipelines();
    abandon();
  }
  device_ = device;

  // Camera-only reuse: dual-speed interactive / static present keeps Layout
  // MapFrame but must not re-place + re-upload every BeginFrame.
  if (world_items && overlay_items && !draw_cache_->full_draws.empty()) {
    if (!ensure_pipelines()) {
      return false;
    }
    detail::encode_draws(list, view, frame, draw_cache_->full_draws, camera,
                         color_op, /*close_list=*/true, solid_pipeline_,
                         textured_pipeline_);
    return true;
  }
  if (world_items && !overlay_items && !draw_cache_->world_draws.empty()) {
    if (!ensure_pipelines()) {
      return false;
    }
    detail::encode_draws(list, view, frame, draw_cache_->world_draws, camera,
                         color_op, /*close_list=*/true, solid_pipeline_,
                         textured_pipeline_);
    return true;
  }
  // Shell-churn / opaque-only refresh: first full record only fills
  // full_draws. Reuse it instead of re-placing ~2k china items (~8s Debug).
  if (world_items && !overlay_items && !draw_cache_->full_draws.empty()) {
    if (!ensure_pipelines()) {
      return false;
    }
    detail::encode_draws(list, view, frame, draw_cache_->full_draws, camera,
                         color_op, /*close_list=*/true, solid_pipeline_,
                         textured_pipeline_);
    return true;
  }

  // Pass §2 (plan map2d-frame §2): MapFrame only — no legacy, no palette in GPU.
  // Background is encode clear (background_rgba × background_opacity), not mesh.
  // Item order is Layout painter order; subset filters world vs icon/text overlay.
  // kRaster/kFill/kLine/kCircle/icon/text paths live in detail::place_frame,
  // upload_draws, encode_draws (0xAARRGGBB × DrawItem::opacity; dash in Layout).
  // Phase 2a edge feather for world solids is in place_frame (1px pad).
  // Overlay follows world on the same list. Keep the world buffers until the
  // next frame's world (or full) record.
  if (world_items || !overlay_items) {
    release_uploaded();
  }

  gis::vista::MapFrame subset = frame;
  if (!(world_items && overlay_items)) {
    subset.items.clear();
    for (const gis::vista::DrawItem& item : frame.items) {
      const bool overlay = item.kind == gis::vista::DrawKind::kIcon ||
                           item.kind == gis::vista::DrawKind::kText;
      if (overlay ? overlay_items : world_items) {
        subset.items.push_back(item);
      }
    }
    if (subset.items.empty()) {
      return true;
    }
  }

  const render::rhi::ColorLoadOp load_op =
      world_items ? color_op : render::rhi::ColorLoadOp::kLoad;
  const bool close_list = world_items && overlay_items;

  std::vector<detail::PlacedMesh> meshes = detail::place_frame(subset, view);
  const detail::AtlasLayout layout = detail::pack_atlas(glyphs, subset);
  detail::bind_glyph_layout(&meshes, layout);
  const std::vector<detail::UploadedDraw> draws = detail::upload_draws(
      device, meshes, layout, load_raster, load_icon, &buffers_, &textures_);
  if (!ensure_pipelines()) {
    return false;
  }
  detail::encode_draws(list, view, subset, draws, camera, load_op, close_list,
                       solid_pipeline_, textured_pipeline_);
  if (world_items && overlay_items) {
    draw_cache_->full_draws = draws;
    // world_draws filled on the next world-only interactive record.
  } else if (world_items) {
    draw_cache_->world_draws = draws;
  }
  return true;
}

}  // namespace map
}  // namespace effect
