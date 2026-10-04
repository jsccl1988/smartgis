// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include <windows.h>

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

HWND make_hwnd() {
  return CreateWindowExW(0, L"STATIC", L"gl-texture-test", WS_POPUP, 0, 0, 128,
                         128, nullptr, nullptr, GetModuleHandleW(nullptr),
                         nullptr);
}

}  // namespace

int main() {
  HWND hwnd = make_hwnd();
  expect(hwnd != nullptr, "CreateWindowEx");
  if (!hwnd) {
    return 1;
  }

  scenic::detail::GlRenderDevice* device = new scenic::detail::GlRenderDevice();
  expect(device->Init(hwnd, "gl_texture_test") == kErrNone, "Init");

  // Texture upload + bind.
  scenic::detail::Texture* tex = device->CreateTexture("unit");
  expect(tex != nullptr, "CreateTexture");
  if (tex) {
    expect(tex->Create(4, 4, scenic::detail::RGBA8, false, false) == kErrNone,
           "Texture::Create");
    expect(tex->Lock() == kErrNone, "Lock");
    for (int i = 0; i < 16; ++i) {
      tex->SetPixel4uc(255, 255, 0, 0);
    }
    // Unlock uploads via BuildTexture and frees CPU staging (legacy
    // Texture).
    expect(tex->Unlock() == kErrNone, "Unlock builds GPU texture");
    expect(device->BindTexture(tex) == kErrNone, "BindTexture");
    expect(device->UnbindTexture() == kErrNone, "UnbindTexture");
  }

  // FBO: attach color texture, bind, clear, unbind.
  scenic::detail::FrameBuffer* fbo = device->CreateFrameBuffer();
  expect(fbo != nullptr, "CreateFrameBuffer");
  if (fbo && tex) {
    expect(device->AttachTexture(fbo, tex, scenic::detail::COLOR_ATTACHMENT0) ==
               kErrNone,
           "AttachTexture");
    expect(device->BindFrameBuffer(fbo) == kErrNone, "BindFrameBuffer");
    expect(device->CheckFrameBufferStatus() == scenic::detail::FRAMEBUFFER_COMPLETE,
           "FBO complete");
    expect(device->BeginRender() == kErrNone, "BeginRender FBO");
    device->SetClearColor(scenic::detail::Color(0.f, 1.f, 0.f, 1.f));
    expect(device->Clear(CLR_COLOR) == kErrNone, "Clear FBO");
    expect(device->UnbindFrameBuffer() == kErrNone, "UnbindFrameBuffer");
    expect(device->DestroyFrameBuffer(fbo) == kErrNone,
           "DestroyFrameBuffer");
  }

  // Font + frustum.
  uint font_id = 0;
  expect(device->CreateFont("Arial", 16, 0, FW_NORMAL, false, false, false, 12,
                            font_id) == kErrNone,
         "CreateFont");
  expect(device->DrawText(font_id, 10.f, 10.f, scenic::detail::Color(1, 1, 1, 1),
                          "Hi") == kErrNone,
         "DrawText screen");

  device->MatrixModeSet(scenic::detail::MM_PROJECTION);
  device->MatrixLoadIdentity();
  device->SetPerspective(45.f, 1.f, 0.1f, 100.f);
  device->MatrixModeSet(scenic::detail::MM_MODELVIEW);
  device->MatrixLoadIdentity();
  scenic::detail::Vector3 eye(0, 0, 5), center(0, 0, 0), up(0, 1, 0);
  device->SetViewLookAt(eye, center, up);
  scenic::detail::Frustum frustum;
  expect(device->GetFrustum(frustum) == kErrNone, "GetFrustum");

  // Indexed draw of a textured unit triangle must not crash.
  scenic::detail::VertexBuffer* vb = device->CreateVertexBuffer(
      3, scenic::detail::VF_XYZ | scenic::detail::VF_NORMAL | scenic::detail::VF_DIFFUSE |
             scenic::detail::VF_TEXCOORD);
  scenic::detail::IndexBuffer* ib = device->CreateIndexBuffer(3);
  expect(vb && ib, "Create VB/IB");
  if (vb && ib) {
    vb->Lock();
    vb->Vertex(-0.5f, -0.5f, 0.f);
    vb->Normal(0, 0, 1);
    vb->Diffuse(1, 1, 1);
    vb->TexVertex(0, 1);
    vb->Vertex(0.5f, -0.5f, 0.f);
    vb->Normal(0, 0, 1);
    vb->Diffuse(1, 1, 1);
    vb->TexVertex(1, 1);
    vb->Vertex(0.f, 0.5f, 0.f);
    vb->Normal(0, 0, 1);
    vb->Diffuse(1, 1, 1);
    vb->TexVertex(0.5f, 0);
    vb->Unlock();
    ib->Lock();
    ib->Index(0);
    ib->Index(1);
    ib->Index(2);
    ib->Unlock();
    if (tex) {
      device->BindTexture(tex);
    }
    expect(device->BeginRender() == kErrNone, "BeginRender draw");
    expect(device->DrawIndexedPrimitives(scenic::detail::PT_TRIANGLELIST, vb, ib, 0,
                                         1) == kErrNone,
           "DrawIndexed textured");
    device->EndRender();
    device->SwapBuffers();
  }

  device->Release();
  delete device;
  DestroyWindow(hwnd);

  if (g_fails) {
    std::fprintf(stderr, "gl_texture_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "gl_texture_test: PASS\n");
  return 0;
}
