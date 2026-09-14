// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/pixel_harness.h"

#include <windows.h>
#include <wincodec.h>

#include <vector>

#pragma comment(lib, "windowscodecs.lib")

namespace ui {
namespace views {
namespace {

class ComScope {
 public:
  ComScope() { CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); }
  ~ComScope() { CoUninitialize(); }
};

}  // namespace

bool load_png_bgra(const std::filesystem::path& path, PixelBuffer* out,
                   std::string* error) {
  if (!out) {
    if (error) {
      *error = "null out";
    }
    return false;
  }
  ComScope com;
  IWICImagingFactory* factory = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory));
  if (FAILED(hr) || !factory) {
    if (error) {
      *error = "WIC factory failed";
    }
    return false;
  }

  IWICBitmapDecoder* decoder = nullptr;
  hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr,
                                        GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                        &decoder);
  if (FAILED(hr) || !decoder) {
    factory->Release();
    if (error) {
      *error = "decoder open failed";
    }
    return false;
  }

  IWICBitmapFrameDecode* frame = nullptr;
  hr = decoder->GetFrame(0, &frame);
  if (FAILED(hr) || !frame) {
    decoder->Release();
    factory->Release();
    if (error) {
      *error = "frame decode failed";
    }
    return false;
  }

  UINT width = 0;
  UINT height = 0;
  frame->GetSize(&width, &height);
  if (width == 0 || height == 0) {
    frame->Release();
    decoder->Release();
    factory->Release();
    if (error) {
      *error = "empty image";
    }
    return false;
  }

  IWICFormatConverter* converter = nullptr;
  hr = factory->CreateFormatConverter(&converter);
  if (FAILED(hr) || !converter) {
    frame->Release();
    decoder->Release();
    factory->Release();
    if (error) {
      *error = "format converter failed";
    }
    return false;
  }

  hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                             WICBitmapDitherTypeNone, nullptr, 0.0,
                             WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) {
    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();
    if (error) {
      *error = "converter init failed";
    }
    return false;
  }

  const UINT stride = width * 4u;
  const UINT size = stride * height;
  std::vector<std::uint8_t> pixels(size);
  hr = converter->CopyPixels(nullptr, stride, size, pixels.data());
  converter->Release();
  frame->Release();
  decoder->Release();
  factory->Release();
  if (FAILED(hr)) {
    if (error) {
      *error = "copy pixels failed";
    }
    return false;
  }

  out->width = static_cast<int>(width);
  out->height = static_cast<int>(height);
  out->bgra = std::move(pixels);
  return true;
}

bool write_png_bgra(const std::filesystem::path& path,
                    const PixelBuffer& buffer, std::string* error) {
  if (buffer.width <= 0 || buffer.height <= 0 ||
      buffer.bgra.size() !=
          static_cast<size_t>(buffer.width) * static_cast<size_t>(buffer.height) *
              4u) {
    if (error) {
      *error = "invalid buffer";
    }
    return false;
  }
  ComScope com;
  IWICImagingFactory* factory = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory));
  if (FAILED(hr) || !factory) {
    if (error) {
      *error = "WIC factory failed";
    }
    return false;
  }

  IWICStream* stream = nullptr;
  hr = factory->CreateStream(&stream);
  if (FAILED(hr) || !stream) {
    factory->Release();
    if (error) {
      *error = "stream create failed";
    }
    return false;
  }
  hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
  if (FAILED(hr)) {
    stream->Release();
    factory->Release();
    if (error) {
      *error = "stream open failed";
    }
    return false;
  }

  IWICBitmapEncoder* encoder = nullptr;
  hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
  if (FAILED(hr) || !encoder) {
    stream->Release();
    factory->Release();
    if (error) {
      *error = "encoder create failed";
    }
    return false;
  }
  hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
  if (FAILED(hr)) {
    encoder->Release();
    stream->Release();
    factory->Release();
    if (error) {
      *error = "encoder init failed";
    }
    return false;
  }

  IWICBitmapFrameEncode* frame = nullptr;
  IPropertyBag2* props = nullptr;
  hr = encoder->CreateNewFrame(&frame, &props);
  if (FAILED(hr) || !frame) {
    if (props) {
      props->Release();
    }
    encoder->Release();
    stream->Release();
    factory->Release();
    if (error) {
      *error = "new frame failed";
    }
    return false;
  }
  hr = frame->Initialize(props);
  if (props) {
    props->Release();
  }
  if (FAILED(hr)) {
    frame->Release();
    encoder->Release();
    stream->Release();
    factory->Release();
    if (error) {
      *error = "frame init failed";
    }
    return false;
  }

  hr = frame->SetSize(static_cast<UINT>(buffer.width),
                      static_cast<UINT>(buffer.height));
  if (FAILED(hr)) {
    frame->Release();
    encoder->Release();
    stream->Release();
    factory->Release();
    if (error) {
      *error = "set size failed";
    }
    return false;
  }

  WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
  hr = frame->SetPixelFormat(&format);
  if (FAILED(hr)) {
    frame->Release();
    encoder->Release();
    stream->Release();
    factory->Release();
    if (error) {
      *error = "set pixel format failed";
    }
    return false;
  }

  const UINT stride = static_cast<UINT>(buffer.width) * 4u;
  const UINT size = stride * static_cast<UINT>(buffer.height);
  hr = frame->WritePixels(static_cast<UINT>(buffer.height), stride, size,
                          const_cast<BYTE*>(buffer.bgra.data()));
  if (FAILED(hr)) {
    frame->Release();
    encoder->Release();
    stream->Release();
    factory->Release();
    if (error) {
      *error = "write pixels failed";
    }
    return false;
  }

  hr = frame->Commit();
  frame->Release();
  if (FAILED(hr)) {
    encoder->Release();
    stream->Release();
    factory->Release();
    if (error) {
      *error = "frame commit failed";
    }
    return false;
  }

  hr = encoder->Commit();
  encoder->Release();
  stream->Release();
  factory->Release();
  if (FAILED(hr)) {
    if (error) {
      *error = "encoder commit failed";
    }
    return false;
  }
  return true;
}

}  // namespace views
}  // namespace ui
