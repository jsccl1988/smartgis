// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/map/detail/keyed_slices.h"

#include <algorithm>
#include <iterator>
#include <unordered_set>

#include "vista/pass/map/detail/place_upload.h"

namespace vista {
namespace detail {
namespace {

struct Run {
  uint64_t key = 0;
  size_t begin = 0;
  size_t end = 0;
  uint64_t hash = 0;
};

uint64_t hash_span(const vista::MapIR& frame, size_t begin, size_t end) {
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
}

std::vector<Run> collect_runs(const vista::MapIR& frame, bool world_items,
                              bool overlay_items) {
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
      const bool next_overlay =
          frame.items[end].kind == vista::DrawKind::kIcon ||
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
    run.hash = key == 0 ? 0 : hash_span(frame, i, end);
    runs.push_back(run);
    i = end;
  }
  return runs;
}

void destroy_draws(render::rhi::Device* device, DrawCache* cache,
                   std::vector<UploadedDraw> draws,
                   std::vector<render::rhi::Buffer*>* buffers,
                   std::vector<render::rhi::Texture*>* textures) {
  std::unordered_set<render::rhi::Buffer*> drop_buffers;
  std::unordered_set<render::rhi::Texture*> drop_textures;
  for (const UploadedDraw& draw : draws) {
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
  for (const auto& entry : cache->slices) {
    for (const UploadedDraw& draw : entry.second.draws) {
      drop_buffers.erase(draw.vertices);
      drop_buffers.erase(draw.indices);
      drop_textures.erase(draw.texture);
    }
  }
  if (device) {
    for (render::rhi::Buffer* buffer : drop_buffers) {
      device->destroy_buffer(buffer);
    }
    for (render::rhi::Texture* texture : drop_textures) {
      device->destroy_texture(texture);
    }
  }
  buffers->erase(std::remove_if(buffers->begin(), buffers->end(),
                                [&](render::rhi::Buffer* buffer) {
                                  return drop_buffers.count(buffer) != 0;
                                }),
                 buffers->end());
  textures->erase(std::remove_if(textures->begin(), textures->end(),
                                 [&](render::rhi::Texture* texture) {
                                   return drop_textures.count(texture) != 0;
                                 }),
                  textures->end());
}

}  // namespace

bool upload_keyed_slices(
    render::rhi::Device* device, DrawCache* cache, const vista::MapIR& frame,
    const vista::View& view, GlyphRasterizer* glyphs, bool world_items,
    bool overlay_items,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster,
    const std::function<bool(const std::string& symbol_id,
                             std::vector<uint8_t>* rgba, int* w, int* h)>&
        load_icon,
    std::vector<render::rhi::Buffer*>* buffers,
    std::vector<render::rhi::Texture*>* textures,
    std::vector<UploadedDraw>* out_merged) {
  if (!device || !cache || !buffers || !textures || !out_merged) {
    return false;
  }

  const std::vector<Run> runs = collect_runs(frame, world_items, overlay_items);

  std::unordered_set<uint64_t> needed;
  for (const Run& run : runs) {
    if (run.key != 0) {
      needed.insert(run.key);
    }
  }
  std::vector<uint64_t> stale;
  for (const auto& entry : cache->slices) {
    if (needed.count(entry.first) == 0) {
      stale.push_back(entry.first);
    }
  }
  for (uint64_t key : stale) {
    auto found = cache->slices.find(key);
    if (found == cache->slices.end()) {
      continue;
    }
    std::vector<UploadedDraw> old = std::move(found->second.draws);
    cache->slices.erase(found);
    destroy_draws(device, cache, std::move(old), buffers, textures);
  }

  out_merged->clear();
  for (const Run& run : runs) {
    if (run.key != 0) {
      auto found = cache->slices.find(run.key);
      if (found != cache->slices.end() && found->second.hash == run.hash &&
          !found->second.draws.empty()) {
        out_merged->insert(out_merged->end(), found->second.draws.begin(),
                           found->second.draws.end());
        continue;
      }
      if (found != cache->slices.end()) {
        std::vector<UploadedDraw> old = std::move(found->second.draws);
        cache->slices.erase(found);
        destroy_draws(device, cache, std::move(old), buffers, textures);
      }
    }
    vista::MapIR subset;
    subset.background_rgba = frame.background_rgba;
    subset.background_opacity = frame.background_opacity;
    subset.items.assign(
        frame.items.begin() + static_cast<std::ptrdiff_t>(run.begin),
        frame.items.begin() + static_cast<std::ptrdiff_t>(run.end));
    // place both kinds in the subset; atlas glyphs follow the outer
    // overlay_items latch (nullptr skips glyph pack when world-only).
    std::vector<UploadedDraw> draws = place_and_upload(
        device, subset, view, overlay_items ? glyphs : nullptr,
        /*world_items=*/true, /*overlay_items=*/true, load_raster, load_icon,
        buffers, textures);
    if (run.key != 0) {
      DrawCache::SliceGpu stored;
      stored.hash = run.hash;
      stored.draws = draws;
      cache->slices[run.key] = std::move(stored);
    }
    out_merged->insert(out_merged->end(),
                       std::make_move_iterator(draws.begin()),
                       std::make_move_iterator(draws.end()));
  }
  return true;
}

}  // namespace detail
}  // namespace vista
