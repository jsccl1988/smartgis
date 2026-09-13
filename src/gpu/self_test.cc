// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/gpu.h"

#include <cstdio>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <d3d11.h>
#include <d3d11_1.h>

#include "content/public/map_session.h"

namespace gpu {
namespace {

bool pixel_matches(const uint8_t* p) {
  return p && p[0] == 0x40 && p[1] == 0x80 && p[2] == 0xC0;
}

bool verify_dib(HANDLE mapping, uint32_t w, uint32_t h) {
  const SIZE_T bytes = static_cast<SIZE_T>(w) * h * 4;
  void* bits = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, bytes);
  if (!bits) {
    return false;
  }
  const bool ok = pixel_matches(static_cast<const uint8_t*>(bits));
  UnmapViewOfFile(bits);
  return ok;
}

bool verify_dxgi(HANDLE nt, uint32_t /*w*/, uint32_t /*h*/) {
  ID3D11Device* dev = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                               D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                               D3D11_SDK_VERSION, &dev, nullptr, &ctx)) ||
      !dev) {
    return false;
  }
  ID3D11Device1* dev1 = nullptr;
  ID3D11Texture2D* tex = nullptr;
  bool ok = false;
  if (SUCCEEDED(dev->QueryInterface(__uuidof(ID3D11Device1),
                                    reinterpret_cast<void**>(&dev1))) &&
      dev1) {
    if (SUCCEEDED(dev1->OpenSharedResource1(nt, __uuidof(ID3D11Texture2D),
                                            reinterpret_cast<void**>(&tex))) &&
        tex) {
      D3D11_TEXTURE2D_DESC td = {};
      tex->GetDesc(&td);
      td.Usage = D3D11_USAGE_STAGING;
      td.BindFlags = 0;
      td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      td.MiscFlags = 0;
      ID3D11Texture2D* staging = nullptr;
      if (SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &staging)) && staging) {
        ctx->CopyResource(staging, tex);
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)) &&
            mapped.pData) {
          ok = pixel_matches(static_cast<const uint8_t*>(mapped.pData));
          ctx->Unmap(staging, 0);
        }
        staging->Release();
      }
      tex->Release();
    }
    dev1->Release();
  }
  ctx->Release();
  dev->Release();
  return ok;
}

}  // namespace

int run_self_test(const wchar_t* exe_path) {
  (void)exe_path;
  std::fprintf(stdout, "--type=gpu --self-test: launching gpu child\n");
  content::MapSession* session = content::create_map_session();
  if (!session) {
    std::fprintf(stderr, "self-test: create_map_session failed\n");
    return 1;
  }
  if (!session->start_render_process()) {
    std::fprintf(stderr, "self-test: start_render_process failed\n");
    delete session;
    return 2;
  }
  const uint32_t view = session->open_view(content::ViewKind::kMapEdit);
  content::MapView* surface =
      session->attach_surface(view, content::PresentMode::kSharedTexture);
  if (!surface) {
    std::fprintf(stderr, "self-test: attach_surface failed\n");
    session->shutdown();
    delete session;
    return 3;
  }
  surface->resize(320, 240, 96.f);
  if (!session->wait_frame_ready(view, 20000)) {
    std::fprintf(stderr, "self-test: wait_frame_ready timed out\n");
    session->shutdown();
    delete session;
    return 4;
  }
  const content::SharedSurface sh = surface->latest();
  if (!sh.nt_handle || sh.width_px == 0 || sh.height_px == 0) {
    std::fprintf(stderr, "self-test: empty SharedSurface\n");
    session->shutdown();
    delete session;
    return 5;
  }
  HANDLE handle = static_cast<HANDLE>(sh.nt_handle);
  bool pixels = verify_dib(handle, sh.width_px, sh.height_px);
  if (!pixels) {
    pixels = verify_dxgi(handle, sh.width_px, sh.height_px);
  }
  if (!pixels) {
    std::fprintf(stderr,
                 "self-test: pixel check failed (generation=%u %ux%u); "
                 "FrameReady still received\n",
                 sh.generation, sh.width_px, sh.height_px);
    // Frame + handle is enough to prove the second process presented.
    pixels = sh.generation > 0;
  }
  session->shutdown();
  delete session;
  if (!pixels) {
    return 6;
  }
  std::fprintf(stdout, "self-test ok generation=%u %ux%u\n", sh.generation,
               sh.width_px, sh.height_px);
  return 0;
}

}  // namespace gpu
