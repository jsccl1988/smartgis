// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/d3d/host/render_device.h"
#include "scenic/render/rhi3d/impl/d3d/resource/buffer/index_buffer.h"
#include "scenic/render/rhi3d/impl/d3d/resource/buffer/vertex_buffer.h"

namespace scenic {
namespace detail {

long D3dRenderDevice::SetBlending(bool bBlending) {
  m_bBlending = bBlending;
  if (state_manager_) state_manager_->SetBlending(bBlending);
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetBackfaceCulling(RenderStateValue /*rsv*/) {
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetStencilBufferMode(RenderStateValue /*rsv*/,
                                              ulong /*ul*/) {
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetDepthBufferMode(RenderStateValue /*rsv*/) {
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetShadeMode(RenderStateValue /*rsv*/, float,
                                      const Color& /*clr*/) {
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetLight(int index, Light* pLight) {
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
  const Color& diff = pLight->GetDiffuseValue();
  slot.diffuse[0] = diff.fRed;
  slot.diffuse[1] = diff.fGreen;
  slot.diffuse[2] = diff.fBlue;
  const Color& amb = pLight->GetAmbientValue();
  slot.ambient[0] = amb.fRed;
  slot.ambient[1] = amb.fGreen;
  slot.ambient[2] = amb.fBlue;
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetAmbientLight(const Color& clr) {
  scene_ambient_[0] = clr.fRed;
  scene_ambient_[1] = clr.fGreen;
  scene_ambient_[2] = clr.fBlue;
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetTexture(Texture* pTex) {
  if (!pTex) {
    return UnbindTexture();
  }
  return BindTexture(pTex);
}

long D3dRenderDevice::SetMaterial(Material* /*pMat*/) {
  // DEM Terrain uses COLOR_MATERIAL (vertex color); material slots unused.
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetFog(FogMode /*mode*/, const Color& /*color*/,
                                float /*density*/, float /*start*/,
                                float /*end*/) {
  return SMT_ERR_NONE;
}

VertexBuffer* D3dRenderDevice::CreateVertexBuffer(int nCount,
                                                        ulong ulFormat,
                                                        bool bDynamic) {
  return new D3dVertexBuffer(nCount, ulFormat, bDynamic);
}

IndexBuffer* D3dRenderDevice::CreateIndexBuffer(int nCount) {
  return new D3dIndexBuffer(nCount);
}

VideoBuffer* D3dRenderDevice::CreateVideoBuffer(ArrayType /*type*/) {
  return nullptr;
}

long D3dRenderDevice::DestroyBuffer(VideoBuffer* /*buffer*/) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::DestroyIndexBuffer(VideoBuffer* /*buffer*/) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetVertexArray(int, Type, int, void*) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetTextureCoordsArray(int, Type, int, void*) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetNormalArray(Type, int, void*) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetIndexArray(Type, int, void*) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::EnableArray(ArrayType, bool) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::BindBuffer(VideoBuffer*) { return SMT_ERR_FAILURE; }

long D3dRenderDevice::BindIndexBuffer(VideoBuffer*) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::UnbindBuffer() { return SMT_ERR_FAILURE; }

long D3dRenderDevice::UnbindIndexBuffer() { return SMT_ERR_FAILURE; }

long D3dRenderDevice::UpdateBuffer(VideoBuffer*, void*, uint,
                                      VideoBufferStoreMethod) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::UpdateIndexBuffer(VideoBuffer*, void*, uint,
                                           VideoBufferStoreMethod) {
  return SMT_ERR_FAILURE;
}

void* D3dRenderDevice::MapBuffer(VideoBuffer*, AccessMode) {
  return nullptr;
}

long D3dRenderDevice::UnmapBuffer(VideoBuffer*) {
  return SMT_ERR_FAILURE;
}

void* D3dRenderDevice::MapIndexBuffer(VideoBuffer*, AccessMode) {
  return nullptr;
}

long D3dRenderDevice::UnmapIndexBuffer(VideoBuffer*) {
  return SMT_ERR_FAILURE;
}

Shader* D3dRenderDevice::CreateVertexShader(const char*) {
  return nullptr;
}

Shader* D3dRenderDevice::CreatePixelShader(const char*) {
  return nullptr;
}

long D3dRenderDevice::DestroyShader(const char*) { return SMT_ERR_FAILURE; }

Shader* D3dRenderDevice::GetShader(const char*) { return nullptr; }

Program* D3dRenderDevice::CreateProgram(const char*) { return nullptr; }

long D3dRenderDevice::DestroyProgram(const char*) { return SMT_ERR_FAILURE; }

Program* D3dRenderDevice::GetProgram(const char*) { return nullptr; }

long D3dRenderDevice::LoadShaderSource(Shader*, char*) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::CompileShader(Shader*) { return SMT_ERR_FAILURE; }

long D3dRenderDevice::IsShaderCompiled(Shader*) {
  return SMT_ERR_FAILURE;
}

char* D3dRenderDevice::GetShaderLog(Shader*) { return nullptr; }

long D3dRenderDevice::BindProgram(Program*) { return SMT_ERR_FAILURE; }

long D3dRenderDevice::UnbindProgram() { return SMT_ERR_FAILURE; }

long D3dRenderDevice::SetProgramVertexShader(Program*, Shader*) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetProgramPixelShader(Program*, Shader*) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::LinkProgram(Program*) { return SMT_ERR_FAILURE; }

long D3dRenderDevice::IsProgramLinked(Program*) {
  return SMT_ERR_FAILURE;
}

char* D3dRenderDevice::GetProgramLinkLog(Program*) { return nullptr; }

long D3dRenderDevice::SetProgramVector(Program*, string&,
                                          const Vector4&) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetProgramVector(Program*, string&,
                                          const Vector3&) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetProgramVector(Program*, string&,
                                          const Vector2&) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetProgramFloat(Program*, string&, float) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetProgramInt(Program*, string&, int) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::GetProgramFloat(Program*, string&, float*) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::SetProgramTexture(Program*, string&, int) {
  return SMT_ERR_FAILURE;
}

}  // namespace detail
}  // namespace scenic
