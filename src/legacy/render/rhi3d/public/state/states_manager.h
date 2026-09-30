
// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_STATESMANAGER_H
#define _RD3D_STATESMANAGER_H

#include <stack>

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_defs.h"
#include "legacy/render/rhi3d/public/state/states.h"
#include "legacy/render/rhi3d/public/texture/texture.h"

namespace render {
/**
Defines different states of the pipeline that could
be set via StateManager class.
*/
enum PipelineState {
  /* Rasterizer states */
  ALPHA_TEST_STATE = 1 << 0,
  DEPTH_TEST_STATE = 1 << 1,
  BLEND_STATE = 1 << 2,
  COLOR_STATE = 1 << 3,

  /* Transform states */
  VIEWPORT_STATE = 1 << 4,
  MATRIX_STATE = 1 << 5,

  /* Group of the states */
  RASTERIZER_STATE =
      ALPHA_TEST_STATE | DEPTH_TEST_STATE | BLEND_STATE | COLOR_STATE,
  TRANSFORM_STATE = VIEWPORT_STATE | MATRIX_STATE,

  /* Pipeline state */
  GPU_STATE = RASTERIZER_STATE | TRANSFORM_STATE
};

/**
Manages all pipeline states.
*/
class LEGACY_RENDER_EXPORT SmtGPUStateManager {
 public:
  SmtGPUState *GetState();
  virtual void PushStates(uint flags);
  virtual void PopStates(uint flags);

  // alpha test
  virtual SmtAlphaTestState GetAlphaTestState(void) = 0;
  virtual long SetAlphaTestState(SmtAlphaTestState &state) = 0;
  virtual long SetAlphaTest(bool enabled) = 0;
  virtual long SetAlphaTestFunc(Comparison func, float ref) = 0;

  // depth test
  virtual SmtDepthTestState GetDepthTestState() = 0;
  virtual long SetDepthTestState(SmtDepthTestState &state) = 0;
  virtual long SetDepthTest(bool enabled) = 0;
  virtual long SetDepthTestFunc(Comparison func) = 0;

  // blending
  virtual SmtBlendState GetBlendState(void) = 0;
  virtual long SetBlendState(SmtBlendState &state) = 0;
  virtual long SetBlending(bool enabled) = 0;

  // viewport
  virtual Viewport3D GetViewportState(void) = 0;
  virtual long SetViewportState(Viewport3D &vp) = 0;
  virtual long SetViewport(int x, int y, int width, int height);

  // clr
  virtual SmtColor GetColorState(void) = 0;
  virtual long SetColorState(SmtColor &colorState) = 0;
  virtual long SetColor(float red, float green, float blue, float alpha = 1.0);

  // texture
  virtual long Set2DTextures(bool enabled) = 0;
  virtual long Set2DRectTextures(bool enabled) = 0;
  virtual long SetSampler(TextureSampler &sampler) = 0;
  virtual long SetRectSampler(TextureSampler &sampler) = 0;
  virtual long SetTextureEnvironment(TextureEnvMode &envMode) = 0;

  // matrix
  virtual SmtMatrixState GetMatrixState(void);
  virtual long SetMatrixState(SmtMatrixState &state);
  virtual Matrix GetWorldViewMatrix(void) = 0;
  virtual Matrix GetProjectionMatrix(void) = 0;

  virtual long SetWorldViewMatrix(Matrix &matrix) = 0;
  virtual long SetProjectionMatrix(Matrix &matrix) = 0;

  // util
  virtual long GetClearColorValue(float &red, float &green, float &blue,
                                  float &alpha) = 0;
  virtual long SetClearColorValue(float red, float green, float blue,
                                  float alpha = 1.f) = 0;
  virtual long GetClearDepthValue(float &depth) = 0;
  virtual long SetClearDepthValue(float depth) = 0;
  virtual long GetStencilClearValue(ulong &s) = 0;
  virtual long SetStencilClearValue(ulong s) = 0;

  virtual long SetPolygonMode(FaceMode face, PolygonMode mode) = 0;

  virtual long GetLineWidth(float &size) = 0;
  virtual long SetLineWidth(float size) = 0;
  virtual long GetPointSize(float &size) = 0;
  virtual long SetPointSize(float size) = 0;

  virtual long EnableDepthOffset(PolygonMode mode, bool enabled) = 0;
  virtual long DepthOffsetParams(float rFactor, float dFactor) = 0;

  virtual long SetMaterail(bool enabled) = 0;
  virtual long SetLight(bool enabled) = 0;

 private:
  stack<SmtAlphaTestState> alphaStack;
  stack<SmtDepthTestState> depthStack;
  stack<SmtBlendState> blendStack;
  stack<Viewport3D> viewportStack;
  stack<SmtColor> colorStack;
  stack<SmtMatrixState> matrixStack;
};

inline SmtGPUState *SmtGPUStateManager::GetState() {
  SmtGPUState *newState = new SmtGPUState();
  newState->viewport = GetViewportState();
  newState->color = GetColorState();
  newState->blend = GetBlendState();
  newState->alphaTest = GetAlphaTestState();
  newState->depthTest = GetDepthTestState();
  return newState;
}

inline void SmtGPUStateManager::PushStates(uint flags) {
  /* Rasterizer states */
  if (flags & ALPHA_TEST_STATE) {
    alphaStack.push(GetAlphaTestState());
  }

  if (flags & DEPTH_TEST_STATE) {
    depthStack.push(GetDepthTestState());
  }

  if (flags & BLEND_STATE) {
    blendStack.push(GetBlendState());
  }

  if (flags & COLOR_STATE) {
    colorStack.push(GetColorState());
  }

  /* Transform states */
  if (flags & VIEWPORT_STATE) {
    viewportStack.push(GetViewportState());
  }

  if (flags & MATRIX_STATE) {
    matrixStack.push(GetMatrixState());
  }
}

inline void SmtGPUStateManager::PopStates(uint flags) {
  /* Rasterizer states */
  if (flags & ALPHA_TEST_STATE) {
    SetAlphaTestState(alphaStack.top());
    alphaStack.pop();
  }

  if (flags & DEPTH_TEST_STATE) {
    SetDepthTestState(depthStack.top());
    depthStack.pop();
  }

  if (flags & BLEND_STATE) {
    SetBlendState(blendStack.top());
    blendStack.pop();
  }

  if (flags & COLOR_STATE) {
    SetColorState(colorStack.top());
    colorStack.pop();
  }

  /* Transform states */
  if (flags & VIEWPORT_STATE) {
    SetViewportState(viewportStack.top());
    viewportStack.pop();
  }

  if (flags & MATRIX_STATE) {
    SetMatrixState(matrixStack.top());
    matrixStack.pop();
  }
}

inline long SmtGPUStateManager::SetViewport(int x, int y, int width,
                                            int height) {
  Viewport3D vp(x, y, width, height, 0, 0, 0);
  return SetViewportState(vp);
}

inline long SmtGPUStateManager::SetColor(float red, float green, float blue,
                                         float alpha) {
  SmtColor color(red, green, blue, alpha);
  return SetColorState(color);
}

inline SmtMatrixState SmtGPUStateManager::GetMatrixState() {
  render::Matrix worldview = GetWorldViewMatrix();
  render::Matrix projection = GetProjectionMatrix();
  return SmtMatrixState(worldview, projection);
}

inline long SmtGPUStateManager::SetMatrixState(SmtMatrixState &state) {
  SetWorldViewMatrix(state.worldview);
  SetProjectionMatrix(state.projection);

  return SMT_ERR_NONE;
}
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD3D_STATESMANAGER_H