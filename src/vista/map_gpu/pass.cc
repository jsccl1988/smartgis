// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map_gpu/pass.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "vista/map_gpu/detail/atlas.h"
#include "vista/map_gpu/detail/encode.h"
#include "vista/map/place.h"
#include "vista/map_gpu/detail/upload.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"

namespace vista {
namespace {

std::atomic<int64_t> g_last_pass_record_ms{0};

uint64_t hash_items(const vista::MapIR& frame, bool overlay_slice) {
  uint64_t h = 14695981039346656037ull;
  auto mix = [&](uint64_t v) {
    h ^= v;
    h *= 1099511628211ull;
  };
  for (const vista::DrawItem& item : frame.items) {
    const bool is_ov = item.kind == vista::DrawKind::kIcon ||
                       item.kind == vista::DrawKind::kText;
    if (is_ov != overlay_slice) {
      continue;
    }
    mix(static_cast<uint64_t>(item.kind));
    mix(item.rgba);
    mix(item.cache_key);
    mix(item.vertices.size());
    mix(item.indices.size());
    if (!item.vertices.empty()) {
      mix(static_cast<uint64_t>(
          static_cast<int32_t>(item.vertices.front().x * 1000.f)));
      mix(static_cast<uint64_t>(
          static_cast<int32_t>(item.vertices.back().y * 1000.f)));
    }
  }
  return h;
}

}  // namespace

int64_t last_pass_record_ms() {
  return g_last_pass_record_ms.load(std::memory_order_relaxed);
}

void reset_last_pass_record_ms() {
  g_last_pass_record_ms.store(0, std::memory_order_relaxed);
}

struct MapPass::DrawCache {
  struct SliceGpu {
    uint64_t hash = 0;
    std::vector<detail::UploadedDraw> draws;
  };
  std::vector<detail::UploadedDraw> world_draws;
  std::vector<detail::UploadedDraw> full_draws;
  uint64_t world_hash = 0;
  uint64_t overlay_hash = 0;
  std::unordered_map<uint64_t, SliceGpu> slices;
};

MapPass::MapPass() : draw_cache_(std::make_unique<DrawCache>()) {}

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
  return solid_pipeline_ != nullptr && textured_pipeline_ != nullptr &&
         multiply_pipeline_ != nullptr;
}

void MapPass::abandon() {
  solid_pipeline_ = nullptr;
  textured_pipeline_ = nullptr;
  multiply_pipeline_ = nullptr;
  buffers_.clear();
  textures_.clear();
  if (draw_cache_) {
    draw_cache_->world_draws.clear();
    draw_cache_->full_draws.clear();
    draw_cache_->world_hash = 0;
    draw_cache_->overlay_hash = 0;
    draw_cache_->slices.clear();
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
    draw_cache_->world_draws.clear();
    draw_cache_->full_draws.clear();
    draw_cache_->world_hash = 0;
    draw_cache_->overlay_hash = 0;
    draw_cache_->slices.clear();
  }
}

void MapPass::invalidate_uploaded() { release_uploaded(); }

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
    draw_cache_ = std::make_unique<DrawCache>();
  }
  BASE_TRACE_EVENT("upload", "map2d.upload");
  if (device_ != nullptr && device_ != device) {
    destroy_pipelines();
    abandon();
  }
  device_ = device;

  const uint64_t world_hash = hash_items(frame, false);
  const uint64_t overlay_hash = hash_items(frame, true);
  const bool hashes_match = world_hash == draw_cache_->world_hash &&
                            overlay_hash == draw_cache_->overlay_hash &&
                            (draw_cache_->world_hash != 0 ||
                             draw_cache_->overlay_hash != 0);
  // kReuseIfCached: camera-only encode (G0 pan). kIncremental: whole-frame
  // reuse when hashes match; otherwise upload only cache_key misses.
  // kReplace: never reuse.
  const bool reuse_ok =
      upload_policy_ == UploadPolicy::kReuseIfCached ||
      (upload_policy_ == UploadPolicy::kIncremental && hashes_match);

  if (reuse_ok && world_items && overlay_items &&
      !draw_cache_->full_draws.empty()) {
    if (!ensure_pipelines()) {
      return false;
    }
    detail::encode_draws(list, view, frame, draw_cache_->full_draws, camera,
                         color_op, /*close_list=*/true, solid_pipeline_,
                         textured_pipeline_, multiply_pipeline_);
    return true;
  }
  if (reuse_ok && world_items && !overlay_items &&
      !draw_cache_->world_draws.empty()) {
    if (!ensure_pipelines()) {
      return false;
    }
    detail::encode_draws(list, view, frame, draw_cache_->world_draws, camera,
                         color_op, /*close_list=*/true, solid_pipeline_,
                         textured_pipeline_, multiply_pipeline_);
    return true;
  }
  if (reuse_ok && world_items && !overlay_items &&
      !draw_cache_->full_draws.empty()) {
    if (!ensure_pipelines()) {
      return false;
    }
    detail::encode_draws(list, view, frame, draw_cache_->full_draws, camera,
                         color_op, /*close_list=*/true, solid_pipeline_,
                         textured_pipeline_, multiply_pipeline_);
    return true;
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
  // Keyed frames upload misses on this device thread. Untagged items stay on
  // the single place+upload path (tests, and kReuseIfCached cold records).
  if (any_keyed && upload_policy_ != UploadPolicy::kReuseIfCached) {
    return upload_keyed_slices(list, frame, view, glyphs, load_raster,
                               load_icon, camera, world_items, overlay_items,
                               color_op, world_hash, overlay_hash);
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
                         multiply_pipeline_);
    if (world_items && overlay_items) {
      draw_cache_->full_draws = empty;
    } else if (world_items) {
      draw_cache_->world_draws = empty;
    }
    draw_cache_->world_hash = world_hash;
    draw_cache_->overlay_hash = overlay_hash;
    return true;
  }

  std::vector<detail::PlacedMesh> meshes =
      detail::place_frame(frame, view, world_items, overlay_items);
  GlyphRasterizer* atlas_glyphs = overlay_items ? glyphs : nullptr;
  const detail::AtlasLayout layout = detail::pack_atlas(atlas_glyphs, frame);
  detail::bind_glyph_layout(&meshes, layout);
  // Move into upload_draws — seal happens once there; avoid double deep-copy.
  const std::vector<detail::UploadedDraw> draws = detail::upload_draws(
      device, std::move(meshes), layout, load_raster, load_icon, &buffers_,
      &textures_);
  if (!ensure_pipelines()) {
    return false;
  }
  detail::encode_draws(list, view, frame, draws, camera, load_op, close_list,
                       solid_pipeline_, textured_pipeline_, multiply_pipeline_);
  if (world_items && overlay_items) {
    draw_cache_->full_draws = draws;
  } else if (world_items) {
    draw_cache_->world_draws = draws;
  }
  draw_cache_->world_hash = world_hash;
  draw_cache_->overlay_hash = overlay_hash;
  return true;
}

bool MapPass::upload_keyed_slices(
    render::rhi::CommandList* list, const vista::MapIR& frame,
    const vista::View& view, GlyphRasterizer* glyphs,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster,
    const std::function<bool(const std::string& symbol_id,
                             std::vector<uint8_t>* rgba, int* w, int* h)>&
        load_icon,
    const render::rhi::CameraMatrices* camera, bool world_items,
    bool overlay_items, render::rhi::ColorLoadOp color_op, uint64_t world_hash,
    uint64_t overlay_hash) {
  if (!draw_cache_) {
    draw_cache_ = std::make_unique<DrawCache>();
  }
  struct Run {
    uint64_t key = 0;
    size_t begin = 0;
    size_t end = 0;
    uint64_t hash = 0;
  };
  auto hash_span = [&](size_t begin, size_t end) {
    uint64_t h = 14695981039346656037ull;
    auto mix = [&](uint64_t v) {
      h ^= v;
      h *= 1099511628211ull;
    };
    mix(static_cast<uint64_t>(end - begin));
    for (size_t i = begin; i < end; ++i) {
      const vista::DrawItem& item = frame.items[i];
      mix(static_cast<uint64_t>(item.kind));
      mix(item.cache_key);
      mix(item.rgba);
      mix(static_cast<uint64_t>(item.vertices.size()));
      mix(static_cast<uint64_t>(item.indices.size()));
      if (!item.vertices.empty()) {
        mix(static_cast<uint64_t>(
            static_cast<int32_t>(item.vertices.front().x * 1000.f)));
        mix(static_cast<uint64_t>(
            static_cast<int32_t>(item.vertices.back().y * 1000.f)));
      }
    }
    return h;
  };
  std::vector<Run> runs;
  for (size_t i = 0; i < frame.items.size();) {
    const vista::DrawItem& item = frame.items[i];
    const bool overlay = item.kind == vista::DrawKind::kIcon ||
                         item.kind == vista::DrawKind::kText;
    if (overlay ? !overlay_items : !world_items) {
      ++i;
      continue;
    }
    const uint64_t key = item.cache_key;
    size_t end = i + 1;
    while (end < frame.items.size() && frame.items[end].cache_key == key) {
      const bool next_overlay = frame.items[end].kind == vista::DrawKind::kIcon ||
                                frame.items[end].kind == vista::DrawKind::kText;
      if (next_overlay ? !overlay_items : !world_items) {
        break;
      }
      ++end;
    }
    Run run;
    run.key = key;
    run.begin = i;
    run.end = end;
    run.hash = key == 0 ? 0 : hash_span(i, end);
    runs.push_back(run);
    i = end;
  }

  std::unordered_set<uint64_t> needed;
  for (const Run& run : runs) {
    if (run.key != 0) {
      needed.insert(run.key);
    }
  }
  auto destroy_draws = [&](std::vector<detail::UploadedDraw> draws) {
    std::unordered_set<render::rhi::Buffer*> drop_buffers;
    std::unordered_set<render::rhi::Texture*> drop_textures;
    for (const detail::UploadedDraw& draw : draws) {
      if (draw.vertices) {
        drop_buffers.insert(draw.vertices);
      }
      if (draw.indices) {
        drop_buffers.insert(draw.indices);
      }
      if (draw.texture) {
        drop_textures.insert(draw.texture);
      }
    }
    for (const auto& entry : draw_cache_->slices) {
      for (const detail::UploadedDraw& draw : entry.second.draws) {
        drop_buffers.erase(draw.vertices);
        drop_buffers.erase(draw.indices);
        drop_textures.erase(draw.texture);
      }
    }
    if (device_) {
      for (render::rhi::Buffer* buffer : drop_buffers) {
        device_->destroy_buffer(buffer);
      }
      for (render::rhi::Texture* texture : drop_textures) {
        device_->destroy_texture(texture);
      }
    }
    buffers_.erase(std::remove_if(buffers_.begin(), buffers_.end(),
                                  [&](render::rhi::Buffer* buffer) {
                                    return drop_buffers.count(buffer) != 0;
                                  }),
                   buffers_.end());
    textures_.erase(std::remove_if(textures_.begin(), textures_.end(),
                                   [&](render::rhi::Texture* texture) {
                                     return drop_textures.count(texture) != 0;
                                   }),
                    textures_.end());
  };
  std::vector<uint64_t> stale;
  for (const auto& entry : draw_cache_->slices) {
    if (needed.count(entry.first) == 0) {
      stale.push_back(entry.first);
    }
  }
  for (uint64_t key : stale) {
    auto found = draw_cache_->slices.find(key);
    if (found == draw_cache_->slices.end()) {
      continue;
    }
    std::vector<detail::UploadedDraw> old = std::move(found->second.draws);
    draw_cache_->slices.erase(found);
    destroy_draws(std::move(old));
  }

  std::vector<detail::UploadedDraw> merged;
  for (const Run& run : runs) {
    if (run.key != 0) {
      auto found = draw_cache_->slices.find(run.key);
      if (found != draw_cache_->slices.end() && found->second.hash == run.hash &&
          !found->second.draws.empty()) {
        merged.insert(merged.end(), found->second.draws.begin(),
                      found->second.draws.end());
        continue;
      }
      if (found != draw_cache_->slices.end()) {
        std::vector<detail::UploadedDraw> old = std::move(found->second.draws);
        draw_cache_->slices.erase(found);
        destroy_draws(std::move(old));
      }
    }
    vista::MapIR subset;
    subset.background_rgba = frame.background_rgba;
    subset.background_opacity = frame.background_opacity;
    subset.items.assign(frame.items.begin() + static_cast<std::ptrdiff_t>(run.begin),
                        frame.items.begin() + static_cast<std::ptrdiff_t>(run.end));
    std::vector<detail::PlacedMesh> meshes =
        detail::place_frame(subset, view, /*world_items=*/true,
                            /*overlay_items=*/true);
    GlyphRasterizer* atlas_glyphs = overlay_items ? glyphs : nullptr;
    const detail::AtlasLayout layout = detail::pack_atlas(atlas_glyphs, subset);
    detail::bind_glyph_layout(&meshes, layout);
    std::vector<detail::UploadedDraw> draws = detail::upload_draws(
        device_, std::move(meshes), layout, load_raster, load_icon, &buffers_,
        &textures_);
    if (run.key != 0) {
      DrawCache::SliceGpu stored;
      stored.hash = run.hash;
      stored.draws = draws;
      draw_cache_->slices[run.key] = std::move(stored);
    }
    merged.insert(merged.end(), std::make_move_iterator(draws.begin()),
                  std::make_move_iterator(draws.end()));
  }

  if (!ensure_pipelines()) {
    return false;
  }
  const render::rhi::ColorLoadOp load_op =
      world_items ? color_op : render::rhi::ColorLoadOp::kLoad;
  const bool close_list = world_items && overlay_items;
  detail::encode_draws(list, view, frame, merged, camera, load_op, close_list,
                       solid_pipeline_, textured_pipeline_, multiply_pipeline_);
  if (world_items && overlay_items) {
    draw_cache_->full_draws = merged;
  } else if (world_items) {
    draw_cache_->world_draws = merged;
  }
  draw_cache_->world_hash = world_hash;
  draw_cache_->overlay_hash = overlay_hash;
  return true;
}

}  // namespace vista
