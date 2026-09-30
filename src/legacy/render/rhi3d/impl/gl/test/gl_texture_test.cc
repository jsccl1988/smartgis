// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/gl/host/render_device.h"

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

  render::SmtGLRenderDevice* device = new render::SmtGLRenderDevice();
  expect(device->Init(hwnd, "gl_texture_test") == SMT_ERR_NONE, "Init");

  // Texture upload + bind.
  render::SmtTexture* tex = device->CreateTexture("unit");
  expect(tex != nullptr, "CreateTexture");
  if (tex) {
    expect(tex->Create(4, 4, render::RGBA8, false, false) == SMT_ERR_NONE,
           "Texture::Create");
    expect(tex->Lock() == SMT_ERR_NONE, "Lock");
    for (int i = 0; i < 16; ++i) {
      tex->SetPixel4uc(255, 255, 0, 0);
    }
    // Unlock uploads via BuildTexture and frees CPU staging (legacy
    // SmtTexture).
    expect(tex->Unlock() == SMT_ERR_NONE, "Unlock builds GPU texture");
    expect(device->BindTexture(tex) == SMT_ERR_NONE, "BindTexture");
    expect(device->UnbindTexture() == SMT_ERR_NONE, "UnbindTexture");
  }

  // FBO: attach color texture, bind, clear, unbind.
  render::SmtFrameBuffer* fbo = device->CreateFrameBuffer();
  expect(fbo != nullptr, "CreateFrameBuffer");
  if (fbo && tex) {
    expect(device->AttachTexture(fbo, tex, render::COLOR_ATTACHMENT0) ==
               SMT_ERR_NONE,
           "AttachTexture");
    expect(device->BindFrameBuffer(fbo) == SMT_ERR_NONE, "BindFrameBuffer");
    expect(device->CheckFrameBufferStatus() == render::FRAMEBUFFER_COMPLETE,
           "FBO complete");
    expect(device->BeginRender() == SMT_ERR_NONE, "BeginRender FBO");
    device->SetClearColor(render::SmtColor(0.f, 1.f, 0.f, 1.f));
    expect(device->Clear(CLR_COLOR) == SMT_ERR_NONE, "Clear FBO");
    expect(device->UnbindFrameBuffer() == SMT_ERR_NONE, "UnbindFrameBuffer");
    expect(device->DestroyFrameBuffer(fbo) == SMT_ERR_NONE,
           "DestroyFrameBuffer");
  }

  // Font + frustum.
  uint font_id = 0;
  expect(device->CreateFont("Arial", 16, 0, FW_NORMAL, false, false, false, 12,
                            font_id) == SMT_ERR_NONE,
         "CreateFont");
  expect(device->DrawText(font_id, 10.f, 10.f, render::SmtColor(1, 1, 1, 1),
                          "Hi") == SMT_ERR_NONE,
         "DrawText screen");

  device->MatrixModeSet(render::MM_PROJECTION);
  device->MatrixLoadIdentity();
  device->SetPerspective(45.f, 1.f, 0.1f, 100.f);
  device->MatrixModeSet(render::MM_MODELVIEW);
  device->MatrixLoadIdentity();
  render::Vector3 eye(0, 0, 5), center(0, 0, 0), up(0, 1, 0);
  device->SetViewLookAt(eye, center, up);
  render::SmtFrustum frustum;
  expect(device->GetFrustum(frustum) == SMT_ERR_NONE, "GetFrustum");

  // Indexed draw of a textured unit triangle must not crash.
  render::SmtVertexBuffer* vb = device->CreateVertexBuffer(
      3, render::VF_XYZ | render::VF_NORMAL | render::VF_DIFFUSE |
             render::VF_TEXCOORD);
  render::SmtIndexBuffer* ib = device->CreateIndexBuffer(3);
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
    expect(device->BeginRender() == SMT_ERR_NONE, "BeginRender draw");
    expect(device->DrawIndexedPrimitives(render::PT_TRIANGLELIST, vb, ib, 0,
                                         1) == SMT_ERR_NONE,
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
