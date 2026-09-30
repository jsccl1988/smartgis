// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_STATESMANAGER_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_STATESMANAGER_H_

#include "legacy/render/rhi3d/impl/d3d/prerequisites.h"
#include "legacy/render/rhi3d/public/state/states_manager.h"

namespace render {

// CPU-side pipeline state cache for the leftover D3D11 device (v1).
class SmtD3DGPUStateManager : public SmtGPUStateManager {
 public:
  SmtD3DGPUStateManager();

  SmtAlphaTestState GetAlphaTestState(void) override;
  long SetAlphaTestState(SmtAlphaTestState& state) override;
  long SetAlphaTest(bool enabled) override;
  long SetAlphaTestFunc(Comparison func, float ref) override;

  SmtDepthTestState GetDepthTestState(void) override;
  long SetDepthTestState(SmtDepthTestState& state) override;
  long SetDepthTest(bool enabled) override;
  long SetDepthTestFunc(Comparison func) override;

  SmtBlendState GetBlendState(void) override;
  long SetBlendState(SmtBlendState& state) override;
  long SetBlending(bool enabled) override;

  Viewport3D GetViewportState(void) override;
  long SetViewportState(Viewport3D& vp) override;

  SmtColor GetColorState(void) override;
  long SetColorState(SmtColor& colorState) override;

  long Set2DTextures(bool enabled) override;
  long Set2DRectTextures(bool enabled) override;
  long SetSampler(TextureSampler& sampler) override;
  long SetRectSampler(TextureSampler& sampler) override;
  long SetTextureEnvironment(TextureEnvMode& envMode) override;

  Matrix GetWorldViewMatrix(void) override;
  Matrix GetProjectionMatrix(void) override;
  long SetWorldViewMatrix(Matrix& matrix) override;
  long SetProjectionMatrix(Matrix& matrix) override;

  long GetClearColorValue(float& red, float& green, float& blue,
                          float& alpha) override;
  long SetClearColorValue(float red, float green, float blue,
                          float alpha = 1.f) override;
  long GetClearDepthValue(float& depth) override;
  long SetClearDepthValue(float depth) override;
  long GetStencilClearValue(ulong& s) override;
  long SetStencilClearValue(ulong s) override;

  long SetPolygonMode(FaceMode face, PolygonMode mode) override;
  long GetLineWidth(float& size) override;
  long SetLineWidth(float size) override;
  long GetPointSize(float& size) override;
  long SetPointSize(float size) override;
  long EnableDepthOffset(PolygonMode mode, bool enabled) override;
  long DepthOffsetParams(float rFactor, float dFactor) override;
  long SetMaterail(bool enabled) override;
  long SetLight(bool enabled) override;

  bool lighting_enabled() const { return lighting_enabled_; }
  bool color_material_enabled() const { return color_material_enabled_; }

 private:
  SmtAlphaTestState alpha_;
  SmtDepthTestState depth_;
  SmtBlendState blend_;
  Viewport3D viewport_;
  SmtColor color_;
  Matrix world_view_;
  Matrix projection_;
  float clear_color_[4];
  float clear_depth_;
  ulong clear_stencil_;
  float line_width_;
  float point_size_;
  bool lighting_enabled_ = false;
  bool color_material_enabled_ = false;
};

}  // namespace render

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_STATESMANAGER_H_
