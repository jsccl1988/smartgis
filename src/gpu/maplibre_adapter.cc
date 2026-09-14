// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/maplibre_adapter.h"

#include "gpu/maplibre_runtime.h"

#include <cstring>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wincodec.h>
#include <windows.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace gpu {
namespace {

constexpr uint8_t k_default_b = 0x40;
constexpr uint8_t k_default_g = 0x80;
constexpr uint8_t k_default_r = 0xC0;

int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

bool parse_hash_rgb(const char* p, uint8_t* b, uint8_t* g, uint8_t* r,
                    uint8_t* a) {
  if (!p || p[0] != '#') {
    return false;
  }
  ++p;
  int n = 0;
  while (p[n] && n < 8) {
    if (hex_nibble(p[n]) < 0) {
      break;
    }
    ++n;
  }
  auto hx = [&](int i) { return hex_nibble(p[i]); };
  if (n == 6) {
    *r = static_cast<uint8_t>((hx(0) << 4) | hx(1));
    *g = static_cast<uint8_t>((hx(2) << 4) | hx(3));
    *b = static_cast<uint8_t>((hx(4) << 4) | hx(5));
    *a = 0xFF;
    return true;
  }
  if (n == 3) {
    *r = static_cast<uint8_t>(hx(0) * 17);
    *g = static_cast<uint8_t>(hx(1) * 17);
    *b = static_cast<uint8_t>(hx(2) * 17);
    *a = 0xFF;
    return true;
  }
  return false;
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

bool decode_tile_bgra(const std::string& bytes, uint32_t dst_w, uint32_t dst_h,
                      std::vector<uint8_t>* dst) {
  if (bytes.size() == 4) {
    const auto* p = reinterpret_cast<const uint8_t*>(bytes.data());
    fill_bgra(dst, dst_w, dst_h, p[0], p[1], p[2], p[3]);
    return true;
  }
  std::vector<uint8_t> tile;
  uint32_t tw = 0;
  uint32_t th = 0;
  if (!decode_png_wic(bytes, &tile, &tw, &th) || tw == 0 || th == 0) {
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

void resolve_background(const char* style_json, uint8_t* b, uint8_t* g,
                        uint8_t* r, uint8_t* a) {
  *b = k_default_b;
  *g = k_default_g;
  *r = k_default_r;
  *a = 0xFF;
  if (!style_json) {
    return;
  }
  const char* key = std::strstr(style_json, "background-color");
  if (!key) {
    return;
  }
  const char* hash = std::strchr(key, '#');
  if (!hash) {
    return;
  }
  parse_hash_rgb(hash, b, g, r, a);
}

std::string format_xyz(const char* tmpl, int z, int x, int y) {
  std::string out = tmpl ? tmpl : "";
  auto replace_all = [&out](const char* from, const std::string& to) {
    const size_t n = std::strlen(from);
    for (size_t pos = 0; (pos = out.find(from, pos)) != std::string::npos;) {
      out.replace(pos, n, to);
      pos += to.size();
    }
  };
  replace_all("{z}", std::to_string(z));
  replace_all("{x}", std::to_string(x));
  replace_all("{y}", std::to_string(y));
  replace_all("{s}", "a");
  return out;
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

  detail::MaplibreStill still;
  if (detail::try_maplibre_still_image(req.style_json, w, h, &still) &&
      still.bgra.size() == static_cast<size_t>(w) * h * 4u) {
    return present->paint_bgra(still.bgra.data(), w * 4);
  }

  uint8_t b = k_default_b;
  uint8_t g = k_default_g;
  uint8_t r = k_default_r;
  uint8_t a = 0xFF;
  resolve_background(req.style_json, &b, &g, &r, &a);

  std::vector<uint8_t> pixels;
  fill_bgra(&pixels, w, h, b, g, r, a);

  if (req.fetch && req.tile_url_template && req.tile_url_template[0]) {
    const std::string url = format_xyz(req.tile_url_template, 0, 0, 0);
    const TileFetchResult res = req.fetch(url);
    if (res.ok && !res.body.empty()) {
      std::vector<uint8_t> raster;
      if (decode_tile_bgra(res.body, w, h, &raster)) {
        pixels.swap(raster);
      }
    }
  }

  return present->paint_bgra(pixels.data(), w * 4);
}

}  // namespace gpu
