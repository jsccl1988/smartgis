// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/map/pass.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

#include "base/core/log.h"
#include "base/trace/event/process_trace.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "vista/pass/map/detail/draw_cache.h"
#include "vista/pass/map/detail/encode.h"
#include "vista/pass/map/detail/keyed_slices.h"
#include "vista/pass/map/detail/place_upload.h"

namespace vista {
namespace {

std::atomic<int64_t> g_last_pass_record_ms{0};

void latch_cache_after_encode(detail::DrawCache* cache,
                              const std::vector<detail::UploadedDraw>& draws,
                              const vista::View& view, bool world_items,
                              bool overlay_items, uint64_t world_hash,
                              uint64_t overlay_hash) {
  if (world_items && overlay_items) {
    cache->full_draws = draws;
    cache->world_draws = detail::world_only_draws(draws);
    cache->placed_view = view;
  } else if (world_items) {
    // World-only must not refresh placed_view — full_draws may still hold
    // overlays from an older extent.
    cache->world_draws = detail::world_only_draws(draws);
  }
  cache->world_hash = world_hash;
  cache->overlay_hash = overlay_hash;
}

}  // namespace

int64_t last_pass_record_ms() {
  return g_last_pass_record_ms.load(std::memory_order_relaxed);
}

void reset_last_pass_record_ms() {
  g_last_pass_record_ms.store(0, std::memory_order_relaxed);
}

MapPass::MapPass() : draw_cache_(std::make_unique<detail::DrawCache>()) {}

MapPass::~MapPass() { abandon(); }

void MapPass::destroy_pipelines() {
  if (device_) {
    device_->destroy_pipeline(solid_pipeline_);
    device_->destroy_pipeline(textured_pipeline_);
    device_->destroy_pipeline(multiply_pipeline_);
  }
  solid_pipeline_ = nullptr;
  textured_pipeline_ = nullptr;
  multiply_pipeline_ = nullptr;
}

bool MapPass::ensure_pipelines() {
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
  if (!multiply_pipeline_) {
    render::rhi::GraphicsPipelineDesc multiply_desc =
        render::programs::textured_pipeline_desc();
    multiply_desc.blend = render::rhi::BlendMode::kMultiply;
    multiply_pipeline_ = device_->create_graphics_pipeline(multiply_desc);
  }
  // Multiply is optional for china map2d (HSHD stays SrcAlpha). Do not fail
  // the whole present when only the multiply PSO is missing.
  if (!solid_pipeline_ || !textured_pipeline_) {
    LOGGING(LOG_ERROR,
            "map2d: ensure_pipelines fail solid=%p textured=%p multiply=%p",
            solid_pipeline_, textured_pipeline_, multiply_pipeline_);
    return false;
  }
  return true;
}

void MapPass::abandon() {
  solid_pipeline_ = nullptr;
  textured_pipeline_ = nullptr;
  multiply_pipeline_ = nullptr;
  buffers_.clear();
  textures_.clear();
  if (draw_cache_) {
    draw_cache_->clear_uploads();
  }
  device_ = nullptr;
}

void MapPass::release_uploaded() {
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
    draw_cache_->clear_uploads();
  }
}

void MapPass::invalidate_uploaded() { release_uploaded(); }

void MapPass::set_encode_scissor(int32_t x, int32_t y, uint32_t w,
                                 uint32_t h) {
  encode_scissor_set_ = w > 0 && h > 0;
  encode_scissor_x_ = x;
  encode_scissor_y_ = y;
  encode_scissor_w_ = w;
  encode_scissor_h_ = h;
}

void MapPass::clear_encode_scissor() {
  encode_scissor_set_ = false;
  encode_scissor_w_ = 0;
  encode_scissor_h_ = 0;
}

bool MapPass::record(
    render::rhi::Device* device, render::rhi::CommandList* list,
    const vista::MapIR& frame, const vista::View& view,
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
    draw_cache_ = std::make_unique<detail::DrawCache>();
  }
  BASE_TRACE_EVENT("upload", "map2d.upload");
  if (device_ != nullptr && device_ != device) {
    destroy_pipelines();
    abandon();
  }
  device_ = device;

  const detail::EncodeScissor* scissor_ptr = nullptr;
  detail::EncodeScissor scissor{};
  if (encode_scissor_set_ && encode_scissor_w_ > 0 && encode_scissor_h_ > 0) {
    scissor.x = encode_scissor_x_;
    scissor.y = encode_scissor_y_;
    scissor.w = encode_scissor_w_;
    scissor.h = encode_scissor_h_;
    scissor_ptr = &scissor;
  }

  detail::DrawCache* cache = draw_cache_.get();
  const uint64_t world_hash = detail::hash_items(frame, false);
  const uint64_t overlay_hash = detail::hash_items(frame, true);
  const bool hashes_match =
      world_hash == cache->world_hash && overlay_hash == cache->overlay_hash &&
      (cache->world_hash != 0 || cache->overlay_hash != 0);
  // kReuseIfCached: camera-only encode (G0 pan). kIncremental: whole-frame
  // reuse when hashes match; otherwise upload only cache_key misses.
  // kReplace: never reuse.
  const bool reuse_ok =
      upload_policy_ == UploadPolicy::kReuseIfCached ||
      (upload_policy_ == UploadPolicy::kIncremental && hashes_match);

  const bool overlays_view_ok = cache->ensure_overlays_for_view(view);

  // Reuse full_draws only when overlays were placed for this view. After a
  // scrub, full_draws is world-only — reusing it would skip labels for a frame.
  if (reuse_ok && world_items && overlay_items && !cache->full_draws.empty() &&
      overlays_view_ok &&
      (!detail::frame_has_overlay_items(frame) ||
       detail::has_overlay_draws(cache->full_draws))) {
    if (!ensure_pipelines()) {
      return false;
    }
    detail::encode_draws(list, view, frame, cache->full_draws, camera, color_op,
                         /*close_list=*/true, solid_pipeline_,
                         textured_pipeline_, multiply_pipeline_, scissor_ptr);
    return true;
  }
  if (reuse_ok && world_items && !overlay_items) {
    // Interactive / world-only encode must not redraw cached overlays —
    // they were placed for a prior view and read as huge glyphs on a clear.
    // An empty filter must fall through and place world meshes; encoding
    // nothing still clears the target (blank frame mid-drag).
    const bool same_scale =
        detail::view_scale_matches(cache->placed_view, view);
    const auto pick_world = [&](const std::vector<detail::UploadedDraw>& src) {
      return same_scale ? detail::world_only_draws(src)
                        : detail::world_without_dem_raster(src);
    };
    std::vector<detail::UploadedDraw> world;
    if (!cache->world_draws.empty()) {
      world = pick_world(cache->world_draws);
    }
    if (world.empty() && !cache->full_draws.empty()) {
      world = pick_world(cache->full_draws);
    }
    if (!world.empty()) {
      if (!ensure_pipelines()) {
        return false;
      }
      detail::encode_draws(list, view, frame, world, camera, color_op,
                           /*close_list=*/true, solid_pipeline_,
                           textured_pipeline_, multiply_pipeline_, scissor_ptr);
      return true;
    }
  }

  bool any_keyed = false;
  for (const vista::DrawItem& item : frame.items) {
    const bool overlay = item.kind == vista::DrawKind::kIcon ||
                         item.kind == vista::DrawKind::kText;
    if ((overlay ? overlay_items : world_items) && item.cache_key != 0) {
      any_keyed = true;
      break;
    }
  }
  // kIncremental with a warm slice cache: upload only cache_key misses.
  // kReplace cold (and incremental with an empty cache) must NOT take that
  // path — every slice is a miss, and each miss deep-copies DrawItem verts
  // into a subset MapIR before pack (china cold was dominated by those
  // copies). One place+upload fills full_draws; a later SettleRebuild with
  // matching hashes reuses them.
  const bool keyed_cache_warm =
      !cache->slices.empty() || !cache->full_draws.empty();
  // Slice hits keep prior place() verts — skip when the view moved so
  // pixel_space overlays are not replayed at the wrong scale.
  if (any_keyed && upload_policy_ == UploadPolicy::kIncremental &&
      keyed_cache_warm && overlays_view_ok) {
    std::vector<detail::UploadedDraw> merged;
    if (!detail::upload_keyed_slices(device, cache, frame, view, glyphs,
                                     world_items, overlay_items, load_raster,
                                     load_icon, &buffers_, &textures_,
                                     &merged)) {
      return false;
    }
    if (!ensure_pipelines()) {
      return false;
    }
    const render::rhi::ColorLoadOp load_op =
        world_items ? color_op : render::rhi::ColorLoadOp::kLoad;
    const bool close_list = world_items && overlay_items;
    detail::encode_draws(list, view, frame, merged, camera, load_op, close_list,
                         solid_pipeline_, textured_pipeline_,
                         multiply_pipeline_, scissor_ptr);
    latch_cache_after_encode(cache, merged, view, world_items, overlay_items,
                             world_hash, overlay_hash);
    return true;
  }

  // After Interactive pan/zoom the world GPU cache is still valid (lon/lat)
  // but overlays are not. Re-place icons/text only — avoid release_uploaded()
  // which destroys DEM/fill buffers and flashes the swapchain on Settle.
  if (!overlays_view_ok && world_items && overlay_items &&
      !cache->world_draws.empty() &&
      upload_policy_ != UploadPolicy::kReplace) {
    const std::vector<detail::UploadedDraw> overlay_draws =
        detail::place_and_upload(device, frame, view, glyphs,
                                 /*world_items=*/false, /*overlay_items=*/true,
                                 load_raster, load_icon, &buffers_, &textures_,
                                 borrow_raster_);
    if (!ensure_pipelines()) {
      return false;
    }
    std::vector<detail::UploadedDraw> merged =
        detail::world_only_draws(cache->world_draws);
    merged.insert(merged.end(), overlay_draws.begin(), overlay_draws.end());
    detail::encode_draws(list, view, frame, merged, camera, color_op,
                         /*close_list=*/true, solid_pipeline_,
                         textured_pipeline_, multiply_pipeline_, scissor_ptr);
    latch_cache_after_encode(cache, merged, view, /*world_items=*/true,
                             /*overlay_items=*/true, world_hash, overlay_hash);
    return true;
  }

  // MapPass §2: MapIR only — no leftover, no palette in GPU. Background is
  // encode clear. Item order is Layout painter order. Filter world vs
  // icon/text in place_frame so vertex borrows address |frame|.
  if (world_items || !overlay_items) {
    release_uploaded();
  }

  bool any_item = false;
  for (const vista::DrawItem& item : frame.items) {
    const bool overlay = item.kind == vista::DrawKind::kIcon ||
                         item.kind == vista::DrawKind::kText;
    if (overlay ? overlay_items : world_items) {
      any_item = true;
      break;
    }
  }
  const render::rhi::ColorLoadOp load_op =
      world_items ? color_op : render::rhi::ColorLoadOp::kLoad;
  const bool close_list = world_items && overlay_items;

  if (!any_item) {
    if (!world_items || load_op != render::rhi::ColorLoadOp::kClear) {
      return true;
    }
    const std::vector<detail::UploadedDraw> empty;
    detail::encode_draws(list, view, frame, empty, camera, load_op, close_list,
                         solid_pipeline_, textured_pipeline_,
                         multiply_pipeline_, scissor_ptr);
    latch_cache_after_encode(cache, empty, view, world_items, overlay_items,
                             world_hash, overlay_hash);
    return true;
  }

  const std::vector<detail::UploadedDraw> draws = detail::place_and_upload(
      device, frame, view, glyphs, world_items, overlay_items, load_raster,
      load_icon, &buffers_, &textures_, borrow_raster_);
  if (!ensure_pipelines()) {
    return false;
  }
  detail::encode_draws(list, view, frame, draws, camera, load_op, close_list,
                       solid_pipeline_, textured_pipeline_, multiply_pipeline_,
                       scissor_ptr);
  latch_cache_after_encode(cache, draws, view, world_items, overlay_items,
                           world_hash, overlay_hash);
  return true;
}

}  // namespace vista
