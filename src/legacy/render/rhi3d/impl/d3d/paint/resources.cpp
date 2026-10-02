// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/host/render_device.h"
#include "legacy/render/rhi3d/impl/d3d/resource/buffer/index_buffer.h"
#include "legacy/render/rhi3d/impl/d3d/resource/buffer/vertex_buffer.h"

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

long SmtD3DRenderDevice::SetLight(int index, SmtLight* pLight) {
  if (index < 0 || index >= 8) {
    return SMT_ERR_INVALID_PARAM;
  }
  StoredLight& slot = lights_[index];
  if (!pLight) {
    slot.enabled = false;
    return SMT_ERR_NONE;
  }
  slot.enabled = true;
  // Match GL: glLight(GL_POSITION) multiplies by the *current* modelview and
  // stores the result in eye space (not re-transformed each draw).
  const Vector4& pos = pLight->GetPosition();
  const Matrix& m = modelview_;
  float eye[3] = {
      pos.x * m._11 + pos.y * m._21 + pos.z * m._31,
      pos.x * m._12 + pos.y * m._22 + pos.z * m._32,
      pos.x * m._13 + pos.y * m._23 + pos.z * m._33,
  };
  if (eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2] < 1e-12f) {
    eye[0] = pos.x;
    eye[1] = pos.y;
    eye[2] = pos.z;
  }
  slot.eye_dir[0] = eye[0];
  slot.eye_dir[1] = eye[1];
  slot.eye_dir[2] = eye[2];
  const SmtColor& diff = pLight->GetDiffuseValue();
  slot.diffuse[0] = diff.fRed;
  slot.diffuse[1] = diff.fGreen;
  slot.diffuse[2] = diff.fBlue;
  const SmtColor& amb = pLight->GetAmbientValue();
  slot.ambient[0] = amb.fRed;
  slot.ambient[1] = amb.fGreen;
  slot.ambient[2] = amb.fBlue;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetAmbientLight(const SmtColor& clr) {
  scene_ambient_[0] = clr.fRed;
  scene_ambient_[1] = clr.fGreen;
  scene_ambient_[2] = clr.fBlue;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetTexture(SmtTexture* pTex) {
  if (!pTex) {
    return UnbindTexture();
  }
  return BindTexture(pTex);
}

long SmtD3DRenderDevice::SetMaterial(SmtMaterial* /*pMat*/) {
  // DEM SmtTerrain uses COLOR_MATERIAL (vertex color); material slots unused.
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

}  // namespace render
