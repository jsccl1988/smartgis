// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/raster/tile/decode.h"

#include <cstring>

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
namespace detail {
namespace {

// One WIC factory per thread. CoInitializeEx runs once on this thread.
// Stream, decoder, frame, and converter stay per image.
IWICImagingFactory* thread_wic_factory() {
  thread_local bool com_started = false;
  thread_local bool create_attempted = false;
  thread_local IWICImagingFactory* factory = nullptr;
  if (!com_started) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    com_started = true;
  }
  if (!create_attempted) {
    create_attempted = true;
    IWICImagingFactory* created = nullptr;
    const HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                        CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&created));
    if (SUCCEEDED(hr)) {
      factory = created;
    }
  }
  return factory;
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

  IWICImagingFactory* factory = thread_wic_factory();
  if (!factory) {
    return false;
  }
  HRESULT hr = S_OK;

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
  return ok;
}

}  // namespace

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

}  // namespace detail
}  // namespace gpu
