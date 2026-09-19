// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/maplibre_adapter.h"

#include "gpu/maplibre_runtime.h"

#include "net/http/http.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/style_document.h"
#include "gis/tile/source_registry.h"
#include "gis/tile/style_source.h"
#include "gis/tile/tile_provider.h"
#include "gis/tile/xyz_math.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wincodec.h>
#include <windows.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace gpu {
namespace {

constexpr uint8_t k_default_b = 0x40;
constexpr uint8_t k_default_g = 0x80;
constexpr uint8_t k_default_r = 0xC0;

float paint_float(const std::map<std::string, std::string>& paint,
                  const char* key,
                  float fallback) {
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

void fill_bgra(std::vector<uint8_t>* buf, uint32_t w, uint32_t h, uint8_t b,
               uint8_t g, uint8_t r, uint8_t a) {
  buf->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0);
  for (uint32_t i = 0; i < w * h; ++i) {
    (*buf)[i * 4 + 0] = b;
    (*buf)[i * 4 + 1] = g;
    (*buf)[i * 4 + 2] = r;
    (*buf)[i * 4 + 3] = a;
  }
}

// Src-over blend of |src| onto |dst| with an extra layer opacity.
void blend_over(std::vector<uint8_t>* dst, const std::vector<uint8_t>& src,
                float opacity) {
  if (!dst || dst->size() != src.size() || dst->empty()) {
    return;
  }
  const float op = clamp01(opacity);
  for (size_t i = 0; i + 3 < dst->size(); i += 4) {
    const float sb = src[i + 0] / 255.f;
    const float sg = src[i + 1] / 255.f;
    const float sr = src[i + 2] / 255.f;
    const float sa = (src[i + 3] / 255.f) * op;
    const float db = (*dst)[i + 0] / 255.f;
    const float dg = (*dst)[i + 1] / 255.f;
    const float dr = (*dst)[i + 2] / 255.f;
    const float da = (*dst)[i + 3] / 255.f;
    const float out_a = sa + da * (1.f - sa);
    float out_b = 0.f;
    float out_g = 0.f;
    float out_r = 0.f;
    if (out_a > 0.f) {
      out_b = (sb * sa + db * da * (1.f - sa)) / out_a;
      out_g = (sg * sa + dg * da * (1.f - sa)) / out_a;
      out_r = (sr * sa + dr * da * (1.f - sa)) / out_a;
    }
    (*dst)[i + 0] = static_cast<uint8_t>(std::lround(out_b * 255.f));
    (*dst)[i + 1] = static_cast<uint8_t>(std::lround(out_g * 255.f));
    (*dst)[i + 2] = static_cast<uint8_t>(std::lround(out_r * 255.f));
    (*dst)[i + 3] = static_cast<uint8_t>(std::lround(out_a * 255.f));
  }
}

void fill_solid_layer(std::vector<uint8_t>* layer, uint32_t w, uint32_t h,
                      uint32_t argb) {
  const uint8_t a = static_cast<uint8_t>((argb >> 24) & 0xFF);
  const uint8_t r = static_cast<uint8_t>((argb >> 16) & 0xFF);
  const uint8_t g = static_cast<uint8_t>((argb >> 8) & 0xFF);
  const uint8_t b = static_cast<uint8_t>(argb & 0xFF);
  fill_bgra(layer, w, h, b, g, r, a);
}

bool decode_png_wic(const std::string& bytes, std::vector<uint8_t>* bgra,
                    uint32_t* out_w, uint32_t* out_h) {
  if (bytes.size() < 8 || !bgra || !out_w || !out_h) {
    return false;
  }
  const auto* sig = reinterpret_cast<const uint8_t*>(bytes.data());
  const uint8_t png[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if (std::memcmp(sig, png, 8) != 0) {
    return false;
  }

  HRESULT hr_co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool need_uninit = SUCCEEDED(hr_co) || hr_co == S_FALSE;

  IWICImagingFactory* factory = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr) || !factory) {
    if (need_uninit) {
      CoUninitialize();
    }
    return false;
  }

  IWICStream* stream = nullptr;
  hr = factory->CreateStream(&stream);
  IWICBitmapDecoder* decoder = nullptr;
  if (SUCCEEDED(hr) && stream) {
    hr = stream->InitializeFromMemory(
        reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data())),
        static_cast<DWORD>(bytes.size()));
  }
  if (SUCCEEDED(hr) && stream) {
    hr = factory->CreateDecoderFromStream(
        stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
  }
  IWICBitmapFrameDecode* frame = nullptr;
  if (SUCCEEDED(hr) && decoder) {
    hr = decoder->GetFrame(0, &frame);
  }
  IWICFormatConverter* conv = nullptr;
  if (SUCCEEDED(hr) && frame) {
    hr = factory->CreateFormatConverter(&conv);
  }
  if (SUCCEEDED(hr) && conv && frame) {
    hr = conv->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                          WICBitmapDitherTypeNone, nullptr, 0.0,
                          WICBitmapPaletteTypeCustom);
  }
  UINT tw = 0;
  UINT th = 0;
  bool ok = false;
  if (SUCCEEDED(hr) && conv) {
    hr = conv->GetSize(&tw, &th);
  }
  if (SUCCEEDED(hr) && conv && tw > 0 && th > 0) {
    bgra->assign(static_cast<size_t>(tw) * static_cast<size_t>(th) * 4u, 0);
    hr = conv->CopyPixels(nullptr, tw * 4, static_cast<UINT>(bgra->size()),
                          bgra->data());
    if (SUCCEEDED(hr)) {
      *out_w = tw;
      *out_h = th;
      ok = true;
    }
  }
  if (conv) {
    conv->Release();
  }
  if (frame) {
    frame->Release();
  }
  if (decoder) {
    decoder->Release();
  }
  if (stream) {
    stream->Release();
  }
  factory->Release();
  if (need_uninit) {
    CoUninitialize();
  }
  return ok;
}

bool decode_tile_native(const std::string& bytes, std::vector<uint8_t>* bgra,
                        uint32_t* out_w, uint32_t* out_h) {
  if (!bgra || !out_w || !out_h) {
    return false;
  }
  if (bytes.size() == 4) {
    const auto* p = reinterpret_cast<const uint8_t*>(bytes.data());
    bgra->assign({p[0], p[1], p[2], p[3]});
    *out_w = 1;
    *out_h = 1;
    return true;
  }
  return decode_png_wic(bytes, bgra, out_w, out_h);
}

// Stretch a tile to the full present size (legacy no-extent path).
bool decode_tile_bgra(const std::string& bytes, uint32_t dst_w, uint32_t dst_h,
                      std::vector<uint8_t>* dst) {
  std::vector<uint8_t> tile;
  uint32_t tw = 0;
  uint32_t th = 0;
  if (!decode_tile_native(bytes, &tile, &tw, &th) || tw == 0 || th == 0) {
    return false;
  }
  dst->assign(static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h) * 4u, 0);
  for (uint32_t y = 0; y < dst_h; ++y) {
    const uint32_t sy = (y * th) / dst_h;
    for (uint32_t x = 0; x < dst_w; ++x) {
      const uint32_t sx = (x * tw) / dst_w;
      const size_t di = (static_cast<size_t>(y) * dst_w + x) * 4u;
      const size_t si = (static_cast<size_t>(sy) * tw + sx) * 4u;
      (*dst)[di + 0] = tile[si + 0];
      (*dst)[di + 1] = tile[si + 1];
      (*dst)[di + 2] = tile[si + 2];
      (*dst)[di + 3] = tile[si + 3];
    }
  }
  return true;
}

bool extent_is_valid(const content::Extent2& e) {
  return e.xmax > e.xmin && e.ymax > e.ymin;
}

gis::tile::Viewport viewport_from_request(const MapPaintRequest& req) {
  gis::tile::Viewport vp;
  vp.min_x = req.extent.xmin;
  vp.min_y = req.extent.ymin;
  vp.max_x = req.extent.xmax;
  vp.max_y = req.extent.ymax;
  vp.z = req.zoom;
  return vp;
}

std::vector<gis::tile::TileCoord> visible_tile_coords(
    const MapPaintRequest& req) {
  if (!extent_is_valid(req.extent)) {
    return {gis::tile::TileCoord{0, 0, 0}};
  }
  return gis::tile::tiles_for_viewport(viewport_from_request(req));
}

// Map tile world rect into present pixels and overwrite |dst| (same layer).
void blit_tile_world(std::vector<uint8_t>* dst, uint32_t dw, uint32_t dh,
                     const content::Extent2& extent, const base::fRect& world,
                     const std::vector<uint8_t>& tile, uint32_t tw,
                     uint32_t th) {
  if (!dst || dw == 0 || dh == 0 || tw == 0 || th == 0 ||
      tile.size() < static_cast<size_t>(tw) * th * 4u) {
    return;
  }
  const double ex0 = extent.xmin;
  const double ey0 = extent.ymin;
  const double ex1 = extent.xmax;
  const double ey1 = extent.ymax;
  const double span_x = ex1 - ex0;
  const double span_y = ey1 - ey0;
  if (span_x <= 0.0 || span_y <= 0.0) {
    return;
  }
  const double tx0 = static_cast<double>(world.lb.x);
  const double ty0 = static_cast<double>(world.lb.y);
  const double tx1 = static_cast<double>(world.rt.x);
  const double ty1 = static_cast<double>(world.rt.y);
  if (!(tx1 > tx0 && ty1 > ty0)) {
    return;
  }

  const double px0 = (tx0 - ex0) / span_x * static_cast<double>(dw);
  const double px1 = (tx1 - ex0) / span_x * static_cast<double>(dw);
  const double py0 = (ey1 - ty1) / span_y * static_cast<double>(dh);
  const double py1 = (ey1 - ty0) / span_y * static_cast<double>(dh);

  const int ix0 = (std::max)(0, static_cast<int>(std::floor(px0)));
  const int iy0 = (std::max)(0, static_cast<int>(std::floor(py0)));
  const int ix1 =
      (std::min)(static_cast<int>(dw), static_cast<int>(std::ceil(px1)));
  const int iy1 =
      (std::min)(static_cast<int>(dh), static_cast<int>(std::ceil(py1)));

  for (int y = iy0; y < iy1; ++y) {
    for (int x = ix0; x < ix1; ++x) {
      const double wx =
          ex0 + (static_cast<double>(x) + 0.5) / static_cast<double>(dw) *
                    span_x;
      const double wy =
          ey1 - (static_cast<double>(y) + 0.5) / static_cast<double>(dh) *
                    span_y;
      if (wx < tx0 || wx >= tx1 || wy < ty0 || wy >= ty1) {
        continue;
      }
      const double u = (wx - tx0) / (tx1 - tx0);
      const double v = (ty1 - wy) / (ty1 - ty0);
      int sx = static_cast<int>(u * static_cast<double>(tw));
      int sy = static_cast<int>(v * static_cast<double>(th));
      if (sx < 0) {
        sx = 0;
      }
      if (sx >= static_cast<int>(tw)) {
        sx = static_cast<int>(tw) - 1;
      }
      if (sy < 0) {
        sy = 0;
      }
      if (sy >= static_cast<int>(th)) {
        sy = static_cast<int>(th) - 1;
      }
      const size_t di =
          (static_cast<size_t>(y) * dw + static_cast<size_t>(x)) * 4u;
      const size_t si =
          (static_cast<size_t>(sy) * tw + static_cast<size_t>(sx)) * 4u;
      (*dst)[di + 0] = tile[si + 0];
      (*dst)[di + 1] = tile[si + 1];
      (*dst)[di + 2] = tile[si + 2];
      (*dst)[di + 3] = tile[si + 3];
    }
  }
}

bool mosaic_tile_bytes(std::vector<uint8_t>* layer, uint32_t w, uint32_t h,
                       const MapPaintRequest& req,
                       const gis::tile::TileCoord& coord,
                       const std::string& body) {
  if (body.empty()) {
    return false;
  }
  if (!extent_is_valid(req.extent)) {
    return decode_tile_bgra(body, w, h, layer);
  }
  std::vector<uint8_t> tile;
  uint32_t tw = 0;
  uint32_t th = 0;
  if (!decode_tile_native(body, &tile, &tw, &th)) {
    return false;
  }
  const base::fRect world =
      gis::tile::tile_world_rect(coord.z, coord.x, coord.y);
  blit_tile_world(layer, w, h, req.extent, world, tile, tw, th);
  return true;
}

std::vector<std::string> collect_templates(const MapPaintRequest& req) {
  if (!req.tile_url_templates.empty()) {
    return req.tile_url_templates;
  }
  std::vector<std::string> out;
  if (req.tile_url_template && req.tile_url_template[0]) {
    out.emplace_back(req.tile_url_template);
  }
  return out;
}

// Bind Style JSON `sources` into a SourceRegistry (raster XYZ only).
// Injected |fetch| wraps TileProvider so tests need no real HTTP.
gis::tile::SourceRegistry bind_style_source_registry(
    const char* style_json,
    const TileFetchFn& fetch) {
  gis::tile::SourceRegistry registry;
  if (!style_json || !style_json[0]) {
    return registry;
  }
  std::vector<gis::tile::StyleSourceDesc> sources;
  // Fill rasters even when the doc also has vector / unsupported entries.
  (void)gis::tile::parse_style_sources(std::string(style_json), &sources);
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

bool style_has_bindable_raster_sources(const char* style_json) {
  if (!style_json || !style_json[0]) {
    return false;
  }
  std::vector<gis::tile::StyleSourceDesc> sources;
  (void)gis::tile::parse_style_sources(std::string(style_json), &sources);
  for (const auto& desc : sources) {
    if (gis::tile::is_raster_bindable(desc)) {
      return true;
    }
  }
  return false;
}

bool request_has_raster_fetch(const MapPaintRequest& req) {
  if (!req.fetch) {
    return false;
  }
  if (!collect_templates(req).empty()) {
    return true;
  }
  return style_has_bindable_raster_sources(req.style_json);
}

bool paint_from_style_document(detail::PresentTarget* present,
                               const MapPaintRequest& req) {
  const uint32_t w = present->wire().width_px;
  const uint32_t h = present->wire().height_px;
  const size_t nbytes =
      static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;

  gis::style::StyleDocument doc;
  const bool parsed =
      req.style_json &&
      gis::style::parse_style_document(std::string(req.style_json), &doc);

  std::vector<uint8_t> pixels(nbytes, 0);
  // Default solid when style is missing or has no background layer.
  if (!parsed) {
    fill_bgra(&pixels, w, h, k_default_b, k_default_g, k_default_r, 0xFF);
  }

  const std::vector<std::string> templates = collect_templates(req);
  gis::tile::SourceRegistry source_registry =
      bind_style_source_registry(req.style_json, req.fetch);
  size_t request_template_index = 0;

  auto composite_raster_url = [&](const std::string& tmpl, float opacity) {
    if (!req.fetch || tmpl.empty()) {
      return;
    }
    std::vector<uint8_t> layer(nbytes, 0);
    bool any = false;
    const std::vector<gis::tile::TileCoord> coords = visible_tile_coords(req);
    for (const gis::tile::TileCoord& c : coords) {
      const std::string url =
          gis::tile::format_xyz_url(tmpl, c.z, c.x, c.y);
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
      blend_over(&pixels, layer, opacity);
    }
  };

  // Prefer SourceRegistry + TileProvider when the layer binds a source id.
  // With a valid extent, fetch the viewport XYZ set; otherwise keep 0/0/0.
  auto composite_raster_provider =
      [&](const std::shared_ptr<gis::tile::TileProvider>& provider,
          float opacity) {
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
            if (mosaic_tile_bytes(&layer, w, h, req, image.coord,
                                  image.bytes)) {
              any = true;
            }
          }
        }
        if (any) {
          blend_over(&pixels, layer, opacity);
        }
      };

  if (parsed) {
    bool painted_background = false;
    size_t raster_layers = 0;
    for (const auto& layer : doc.layers) {
      if (layer.type == gis::style::LayerType::kBackground) {
        uint32_t argb = 0xFF000000u | (static_cast<uint32_t>(k_default_r) << 16) |
                        (static_cast<uint32_t>(k_default_g) << 8) |
                        static_cast<uint32_t>(k_default_b);
        auto cit = layer.paint.find("background-color");
        if (cit != layer.paint.end()) {
          uint32_t parsed_argb = 0;
          if (gis::style::parse_color(cit->second, &parsed_argb)) {
            argb = parsed_argb;
          }
        }
        const float opacity =
            clamp01(paint_float(layer.paint, "background-opacity", 1.f));
        std::vector<uint8_t> solid;
        fill_solid_layer(&solid, w, h, argb);
        if (!painted_background && opacity >= 1.f &&
            ((argb >> 24) & 0xFF) == 0xFF) {
          pixels.swap(solid);
        } else {
          blend_over(&pixels, solid, opacity);
        }
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
        composite_raster_provider(provider, opacity);
        continue;
      }

      // No source id: fall back to request tile_url_templates (document order).
      std::string tmpl;
      if (request_template_index < templates.size()) {
        tmpl = templates[request_template_index];
      }
      ++request_template_index;
      composite_raster_url(tmpl, opacity);
    }

    // Request templates without matching raster layers: treat each remaining
    // (or all) template as an opaque raster pass so SMT_XYZ_URL / callers keep
    // working with a background-only Style JSON.
    if (req.fetch && !templates.empty() && raster_layers == 0) {
      for (const auto& tmpl : templates) {
        composite_raster_url(tmpl, 1.f);
      }
    }

    if (!painted_background && pixels.size() == nbytes) {
      // Parsed style with no background layer: keep transparent black only if
      // rasters painted; otherwise fall back to the historical default solid.
      bool any_non_zero = false;
      for (uint8_t v : pixels) {
        if (v != 0) {
          any_non_zero = true;
          break;
        }
      }
      if (!any_non_zero) {
        fill_bgra(&pixels, w, h, k_default_b, k_default_g, k_default_r, 0xFF);
      }
    }
  } else if (req.fetch && !templates.empty()) {
    // Unparsed style: preserve prior single-raster cover behavior.
    composite_raster_url(templates[0], 1.f);
  }

  return present->paint_bgra(pixels.data(), w * 4);
}

}  // namespace

bool paint_track_a_basemap(detail::PresentTarget* present,
                           const MapPaintRequest& req) {
  if (!present) {
    return false;
  }
  const uint32_t w = present->wire().width_px;
  const uint32_t h = present->wire().height_px;
  if (w == 0 || h == 0) {
    return false;
  }

  // Division of labor with mln::Map still-image:
  // - Facade: background (+ opacity) only; no TileFetchFn / multi-raster.
  // - Adapter: authoritative Track A richness (StyleDocument + rasters).
  // Prefer the facade only on background-only requests so the pin stays
  // exercised without stealing multi-layer compositing.
  if (!request_has_raster_fetch(req)) {
    detail::MaplibreStill still;
    if (detail::try_maplibre_still_image(req.style_json, w, h, &still) &&
        still.bgra.size() == static_cast<size_t>(w) * h * 4u) {
      return present->paint_bgra(still.bgra.data(), w * 4);
    }
  }

  return paint_from_style_document(present, req);
}

}  // namespace gpu
