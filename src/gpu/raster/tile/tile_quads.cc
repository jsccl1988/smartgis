// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/raster/tile/tile_quads.h"

#include "gpu/compositor/composer/software_composer.h"
#include "gpu/raster/tile/mosaic.h"
#include "gis/present/style/paint_resolve.h"
#include "gis/present/style/style_document.h"
#include "gis/present/tile/provider/source_registry.h"
#include "gis/present/tile/provider/style_source.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "net/http/http.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gpu {
namespace detail {
namespace {

constexpr uint8_t k_default_b = 0x40;
constexpr uint8_t k_default_g = 0x80;
constexpr uint8_t k_default_r = 0xC0;

uint32_t default_argb() {
  return 0xFF000000u | (static_cast<uint32_t>(k_default_r) << 16) |
         (static_cast<uint32_t>(k_default_g) << 8) |
         static_cast<uint32_t>(k_default_b);
}

float paint_float(const std::map<std::string, std::string>& paint,
                  const char* key, float fallback) {
  auto it = paint.find(key);
  if (it == paint.end()) {
    return fallback;
  }
  char* stop = nullptr;
  const float v = static_cast<float>(std::strtod(it->second.c_str(), &stop));
  if (stop == it->second.c_str()) {
    return fallback;
  }
  return v;
}

float clamp01(float v) {
  return (std::max)(0.f, (std::min)(1.f, v));
}

std::vector<std::string> collect_templates(const DrawRequest& req) {
  if (!req.tile_url_templates.empty()) {
    return req.tile_url_templates;
  }
  std::vector<std::string> out;
  if (req.tile_url_template && req.tile_url_template[0]) {
    out.emplace_back(req.tile_url_template);
  }
  return out;
}

// Parsed Style text. Entries stay for the process; a new string parses again.
struct CachedStyleParse {
  bool document_ok = false;
  gis::style::StyleDocument document;
  std::vector<gis::tile::StyleSourceDesc> sources;
};

// Process-local cache keyed by style text. Null and empty styles do not
// enter here.
void copy_cached_style_parse(
    const std::string& style, gis::style::StyleDocument* doc, bool* document_ok,
    std::vector<gis::tile::StyleSourceDesc>* sources) {
  static std::mutex mu;
  static std::map<std::string, CachedStyleParse> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(style);
  if (it == cache.end()) {
    CachedStyleParse parsed;
    parsed.document_ok =
        gis::style::parse_style_document(style, &parsed.document);
    // Fill rasters even when the doc also has vector / unsupported entries.
    (void)gis::tile::parse_style_sources(style, &parsed.sources);
    it = cache.emplace(style, std::move(parsed)).first;
  }
  *doc = it->second.document;
  *document_ok = it->second.document_ok;
  *sources = it->second.sources;
}

// Bind already-parsed Style `sources` into a SourceRegistry (raster XYZ only).
// Injected |fetch| wraps TileProvider so tests need no real HTTP.
gis::tile::SourceRegistry bind_style_source_registry(
    const std::vector<gis::tile::StyleSourceDesc>& sources,
    const TileFetchFn& fetch) {
  gis::tile::SourceRegistry registry;
  for (const auto& desc : sources) {
    if (registry.bind_raster(desc) != gis::tile::StyleSourceStatus::kOk) {
      continue;
    }
    if (!fetch) {
      continue;
    }
    auto provider = registry.get(desc.id);
    if (!provider) {
      continue;
    }
    provider->set_fetch_fn([fetch](const std::string& url) {
      net::HttpResult hr;
      const TileFetchResult res = fetch(url);
      hr.ok = res.ok;
      hr.status = res.ok ? 200 : 0;
      hr.body = res.body;
      return hr;
    });
  }
  return registry;
}

bool pass_is_all_zero(const RenderPass& pass) {
  std::vector<uint8_t> pixels;
  if (!blend_render_pass(pass, pass.width_px, pass.height_px, &pixels)) {
    return true;
  }
  for (uint8_t v : pixels) {
    if (v != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool raster_tile_quads(OutputSurface* surface, const DrawRequest& req,
                       RenderPass* pass) {
  if (!surface || !pass) {
    return false;
  }
  const uint32_t w = surface->wire().width_px;
  const uint32_t h = surface->wire().height_px;
  if (w == 0 || h == 0) {
    return false;
  }
  pass->width_px = w;
  pass->height_px = h;
  const size_t nbytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;

  gis::style::StyleDocument doc;
  bool parsed = false;
  std::vector<gis::tile::StyleSourceDesc> sources;
  if (req.style_json && req.style_json[0]) {
    copy_cached_style_parse(std::string(req.style_json), &doc, &parsed,
                            &sources);
  } else if (req.style_json) {
    parsed = gis::style::parse_style_document(std::string(req.style_json), &doc);
  }

  if (!parsed) {
    append_solid_quad(pass, default_argb(), 1.f);
  }

  const std::vector<std::string> templates = collect_templates(req);
  gis::tile::SourceRegistry source_registry =
      bind_style_source_registry(sources, req.fetch);
  size_t request_template_index = 0;

  auto composite_raster_url = [&](const std::string& tmpl, float opacity,
                                  size_t layer_index) {
    if (!req.fetch || tmpl.empty()) {
      return;
    }
    std::vector<uint8_t> layer(nbytes, 0);
    bool any = false;
    const std::vector<gis::tile::TileCoord> coords = visible_tile_coords(req);
    for (const gis::tile::TileCoord& c : coords) {
      const std::string url = gis::tile::format_xyz_url(tmpl, c.z, c.x, c.y);
      const TileFetchResult res = req.fetch(url);
      if (!res.ok || res.body.empty()) {
        continue;
      }
      if (!extent_is_valid(req.extent)) {
        if (decode_tile_bgra(res.body, w, h, &layer)) {
          any = true;
        }
        break;
      }
      if (mosaic_tile_bytes(&layer, w, h, req, c, res.body)) {
        any = true;
      }
    }
    if (any) {
          uint64_t key = 14695981039346656037ull;
          for (unsigned char ch : tmpl) {
            key ^= ch;
            key *= 1099511628211ull;
          }
          key ^= static_cast<uint64_t>(req.zoom) << 32;
          key ^= static_cast<uint64_t>(layer_index);
          auto mix_extent = [&key](double v) {
            uint64_t bits = 0;
            static_assert(sizeof(double) == sizeof(uint64_t), "double size");
            std::memcpy(&bits, &v, sizeof(bits));
            key ^= bits;
            key *= 1099511628211ull;
          };
          mix_extent(req.extent.xmin);
          mix_extent(req.extent.ymin);
          mix_extent(req.extent.xmax);
          mix_extent(req.extent.ymax);
          if (key == 0) {
            key = 1;
          }
          append_bgra_quad(pass, std::move(layer), opacity, false, key);
    }
  };

  // Prefer SourceRegistry + TileProvider when the layer binds a source id.
  // With a valid extent, fetch the viewport XYZ set; otherwise keep 0/0/0.
  auto composite_raster_provider =
      [&](const std::shared_ptr<gis::tile::TileProvider>& provider,
          float opacity, size_t layer_index) {
        if (!req.fetch || !provider || !provider->is_open()) {
          return;
        }
        std::vector<uint8_t> layer(nbytes, 0);
        bool any = false;
        if (!extent_is_valid(req.extent)) {
          const gis::tile::TileImage image =
              provider->fetch_tile(gis::tile::TileCoord{0, 0, 0});
          if (decode_tile_bgra(image.bytes, w, h, &layer)) {
            any = true;
          }
        } else {
          const std::vector<gis::tile::TileImage> images =
              provider->fetch_visible(viewport_from_request(req));
          for (const gis::tile::TileImage& image : images) {
            if (image.bytes.empty()) {
              continue;
            }
            if (mosaic_tile_bytes(&layer, w, h, req, image.coord, image.bytes)) {
              any = true;
            }
          }
        }
        if (any) {
          uint64_t key = 14695981039346656037ull;
          key ^= static_cast<uint64_t>(req.zoom) << 32;
          key ^= static_cast<uint64_t>(layer_index);
          auto mix_extent = [&key](double v) {
            uint64_t bits = 0;
            static_assert(sizeof(double) == sizeof(uint64_t), "double size");
            std::memcpy(&bits, &v, sizeof(bits));
            key ^= bits;
            key *= 1099511628211ull;
          };
          mix_extent(req.extent.xmin);
          mix_extent(req.extent.ymin);
          mix_extent(req.extent.xmax);
          mix_extent(req.extent.ymax);
          if (key == 0) {
            key = 1;
          }
          append_bgra_quad(pass, std::move(layer), opacity, false, key);
        }
      };

  if (parsed) {
    bool painted_background = false;
    size_t raster_layers = 0;
    for (const auto& layer : doc.layers) {
      if (layer.type == gis::style::LayerType::kBackground) {
        uint32_t argb = default_argb();
        auto cit = layer.paint.find("background-color");
        if (cit != layer.paint.end()) {
          uint32_t parsed_argb = 0;
          if (gis::style::parse_color(cit->second, &parsed_argb)) {
            argb = parsed_argb;
          }
        }
        const float opacity =
            clamp01(paint_float(layer.paint, "background-opacity", 1.f));
        append_solid_quad(pass, argb, opacity);
        painted_background = true;
        continue;
      }

      if (layer.type != gis::style::LayerType::kRaster) {
        continue;
      }
      ++raster_layers;

      const float opacity =
          clamp01(paint_float(layer.paint, "raster-opacity", 1.f));

      // Style `layer.source` → SourceRegistry id. Missing id → skip layer.
      if (!layer.source.empty()) {
        auto provider = source_registry.get(layer.source);
        if (!provider) {
          continue;
        }
        composite_raster_provider(provider, opacity, raster_layers);
        continue;
      }

      // No source id: fall back to request tile_url_templates (document order).
      std::string tmpl;
      if (request_template_index < templates.size()) {
        tmpl = templates[request_template_index];
      }
      ++request_template_index;
      composite_raster_url(tmpl, opacity, raster_layers);
    }

    // Request templates without matching raster layers: treat each remaining
    // (or all) template as an opaque raster pass so SMT_XYZ_URL / callers keep
    // working with a background-only Style JSON.
    if (req.fetch && !templates.empty() && raster_layers == 0) {
      size_t i = 0;
      for (const auto& tmpl : templates) {
        composite_raster_url(tmpl, 1.f, ++i);
      }
    }

    if (!painted_background && pass_is_all_zero(*pass)) {
      pass->quad_list.clear();
      pass->image_data.clear();
      append_solid_quad(pass, default_argb(), 1.f);
    }
  } else if (req.fetch && !templates.empty()) {
    // Unparsed style: preserve prior single-raster cover behavior.
    composite_raster_url(templates[0], 1.f, 1);
  }

  return true;
}

}  // namespace detail
}  // namespace gpu
