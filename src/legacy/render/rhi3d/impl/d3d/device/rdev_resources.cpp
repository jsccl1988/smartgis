// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/buffer/indexbuffer.h"
#include "legacy/render/rhi3d/impl/d3d/buffer/vertexbuffer.h"
#include "legacy/render/rhi3d/impl/d3d/device/3drenderdevice.h"

namespace render {

long SmtD3DRenderDevice::SetBlending(bool bBlending) {
  m_bBlending = bBlending;
  if (state_manager_) state_manager_->SetBlending(bBlending);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetBackfaceCulling(RenderStateValue /*rsv*/) {
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetStencilBufferMode(RenderStateValue /*rsv*/,
                                              ulong /*ul*/) {
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetDepthBufferMode(RenderStateValue /*rsv*/) {
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetShadeMode(RenderStateValue /*rsv*/, float,
                                      const SmtColor& /*clr*/) {
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetLight(int /*index*/, SmtLight* /*pLight*/) {
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetAmbientLight(const SmtColor& /*clr*/) {
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetTexture(SmtTexture* /*pTex*/) {
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetMaterial(SmtMaterial* /*pMat*/) {
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetFog(FogMode /*mode*/, const SmtColor& /*color*/,
                                float /*density*/, float /*start*/,
                                float /*end*/) {
  return SMT_ERR_NONE;
}

SmtVertexBuffer* SmtD3DRenderDevice::CreateVertexBuffer(int nCount,
                                                        ulong ulFormat,
                                                        bool bDynamic) {
  return new SmtD3DVertexBuffer(nCount, ulFormat, bDynamic);
}

SmtIndexBuffer* SmtD3DRenderDevice::CreateIndexBuffer(int nCount) {
  return new SmtD3DIndexBuffer(nCount);
}

SmtVideoBuffer* SmtD3DRenderDevice::CreateVideoBuffer(ArrayType /*type*/) {
  return nullptr;
}

long SmtD3DRenderDevice::DestroyBuffer(SmtVideoBuffer* /*buffer*/) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::DestroyIndexBuffer(SmtVideoBuffer* /*buffer*/) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetVertexArray(int, Type, int, void*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetTextureCoordsArray(int, Type, int, void*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetNormalArray(Type, int, void*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetIndexArray(Type, int, void*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::EnableArray(ArrayType, bool) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::BindBuffer(SmtVideoBuffer*) { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::BindIndexBuffer(SmtVideoBuffer*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::UnbindBuffer() { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::UnbindIndexBuffer() { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::UpdateBuffer(SmtVideoBuffer*, void*, uint,
                                      VideoBufferStoreMethod) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::UpdateIndexBuffer(SmtVideoBuffer*, void*, uint,
                                           VideoBufferStoreMethod) {
  return SMT_ERR_FAILURE;
}

void* SmtD3DRenderDevice::MapBuffer(SmtVideoBuffer*, AccessMode) {
  return nullptr;
}

long SmtD3DRenderDevice::UnmapBuffer(SmtVideoBuffer*) {
  return SMT_ERR_FAILURE;
}

void* SmtD3DRenderDevice::MapIndexBuffer(SmtVideoBuffer*, AccessMode) {
  return nullptr;
}

long SmtD3DRenderDevice::UnmapIndexBuffer(SmtVideoBuffer*) {
  return SMT_ERR_FAILURE;
}

SmtShader* SmtD3DRenderDevice::CreateVertexShader(const char*) {
  return nullptr;
}

SmtShader* SmtD3DRenderDevice::CreatePixelShader(const char*) {
  return nullptr;
}

long SmtD3DRenderDevice::DestroyShader(const char*) { return SMT_ERR_FAILURE; }

SmtShader* SmtD3DRenderDevice::GetShader(const char*) { return nullptr; }

SmtProgram* SmtD3DRenderDevice::CreateProgram(const char*) { return nullptr; }

long SmtD3DRenderDevice::DestroyProgram(const char*) { return SMT_ERR_FAILURE; }

SmtProgram* SmtD3DRenderDevice::GetProgram(const char*) { return nullptr; }

long SmtD3DRenderDevice::LoadShaderSource(SmtShader*, char*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::CompileShader(SmtShader*) { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::IsShaderCompiled(SmtShader*) {
  return SMT_ERR_FAILURE;
}

char* SmtD3DRenderDevice::GetShaderLog(SmtShader*) { return nullptr; }

long SmtD3DRenderDevice::BindProgram(SmtProgram*) { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::UnbindProgram() { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::SetProgramVertexShader(SmtProgram*, SmtShader*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetProgramPixelShader(SmtProgram*, SmtShader*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::LinkProgram(SmtProgram*) { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::IsProgramLinked(SmtProgram*) {
  return SMT_ERR_FAILURE;
}

char* SmtD3DRenderDevice::GetProgramLinkLog(SmtProgram*) { return nullptr; }

long SmtD3DRenderDevice::SetProgramVector(SmtProgram*, string&,
                                          const Vector4&) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetProgramVector(SmtProgram*, string&,
                                          const Vector3&) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetProgramVector(SmtProgram*, string&,
                                          const Vector2&) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetProgramFloat(SmtProgram*, string&, float) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetProgramInt(SmtProgram*, string&, int) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::GetProgramFloat(SmtProgram*, string&, float*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetProgramTexture(SmtProgram*, string&, int) {
  return SMT_ERR_FAILURE;
}

SmtTexture* SmtD3DRenderDevice::CreateTexture(const char*) { return nullptr; }

long SmtD3DRenderDevice::DestroyTexture(const char*) { return SMT_ERR_FAILURE; }

SmtTexture* SmtD3DRenderDevice::GetTexture(const char*) { return nullptr; }

long SmtD3DRenderDevice::GenerateMipmap(SmtTexture*) { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::BindTexture(SmtTexture*) { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::BuildTexture(SmtTexture*) { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::BindRectTexture(SmtTexture*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::UnbindTexture() { return SMT_ERR_FAILURE; }

long SmtD3DRenderDevice::UnbindRectTexture(void) { return SMT_ERR_FAILURE; }

SmtFrameBuffer* SmtD3DRenderDevice::CreateFrameBuffer() { return nullptr; }

long SmtD3DRenderDevice::DestroyFrameBuffer(SmtFrameBuffer*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::BindFrameBuffer(SmtFrameBuffer*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::UnbindFrameBuffer() { return SMT_ERR_FAILURE; }

SmtRenderBuffer* SmtD3DRenderDevice::CreateRenderBuffer(TextureFormat, uint,
                                                        uint) {
  return nullptr;
}

long SmtD3DRenderDevice::DestroyRenderBuffer(SmtRenderBuffer*) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::AttachRenderBuffer(SmtFrameBuffer*, SmtRenderBuffer*,
                                            RenderBufferSlot) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::AttachTexture(SmtFrameBuffer*, SmtTexture*,
                                       RenderBufferSlot) {
  return SMT_ERR_FAILURE;
}

FrameBufferStatus SmtD3DRenderDevice::CheckFrameBufferStatus() {
  return FRAMEBUFFER_UNSUPPORTED;
}

long SmtD3DRenderDevice::CreateFont(const char*, int, int, int, bool, bool,
                                    bool, ulong, uint&) {
  return SMT_ERR_FAILURE;
}

}  // namespace render
