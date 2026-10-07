
// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_STATESMANAGER_H
#define _RD3D_STATESMANAGER_H

#include <stack>

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_defs.h"
#include "scenic/render/rhi3d/public/state/states.h"
#include "scenic/render/rhi3d/public/texture/texture.h"

namespace scenic {
namespace detail {
/**
Defines different states of the pipeline that could
be set via StateManager class.
*/
enum PipelineState {
  /* Rasterizer states */
  ALPHA_TEST_STATE = 1 << 0,
  DEPTH_TEST_STATE = 1 << 1,
  BLEND_STATE = 1 << 2,
  COLOR_STATE = 1 << 3

  /* Transform states */
  VIEWPORT_STATE = 1 << 4,
  MATRIX_STATE = 1 << 5

  /* Group of the states */
  RASTERIZER_STATE =
      ALPHA_TEST_STATE | DEPTH_TEST_STATE | BLEND_STATE | COLOR_STATE,
  TRANSFORM_STATE = VIEWPORT_STATE | MATRIX_STATE

  /* Pipeline state */
  GPU_STATE = RASTERIZER_STATE | TRANSFORM_STATE
};

/**
Manages all pipeline states.
*/
class SCENIC_IMPL_EXPORT GpuStateManager {
 public:
  GpuState *GetState();
  virtual void PushStates(uint flags);
  virtual void PopStates(uint flags);

  // alpha test
  virtual AlphaTestState GetAlphaTestState(void) = 0;
  virtual long SetAlphaTestState(AlphaTestState &state) = 0;
  virtual long SetAlphaTest(bool enabled) = 0;
  virtual long SetAlphaTestFunc(Comparison func, float ref) = 0;

  // depth test
  virtual DepthTestState GetDepthTestState() = 0;
  virtual long SetDepthTestState(DepthTestState &state) = 0;
  virtual long SetDepthTest(bool enabled) = 0;
  virtual long SetDepthTestFunc(Comparison func) = 0;

  // blending
  virtual BlendState GetBlendState(void) = 0;
  virtual long SetBlendState(BlendState &state) = 0;
  virtual long SetBlending(bool enabled) = 0;

  // viewport
  virtual Viewport3D GetViewportState(void) = 0;
  virtual long SetViewportState(Viewport3D &vp) = 0;
  virtual long SetViewport(int x, int y, int width, int height);

  // clr
  virtual Color GetColorState(void) = 0;
  virtual long SetColorState(Color &colorState) = 0;
  virtual long SetColor(float red, float green, float blue, float alpha = 1.0);

  // texture
  virtual long Set2DTextures(bool enabled) = 0;
  virtual long Set2DRectTextures(bool enabled) = 0;
  virtual long SetSampler(TextureSampler &sampler) = 0;
  virtual long SetRectSampler(TextureSampler &sampler) = 0;
  virtual long SetTextureEnvironment(TextureEnvMode &envMode) = 0;

  // matrix
  virtual MatrixState GetMatrixState(void);
  virtual long SetMatrixState(MatrixState &state);
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
  std::stack<AlphaTestState> alphaStack;
  std::stack<DepthTestState> depthStack;
  std::stack<BlendState> blendStack;
  std::stack<Viewport3D> viewportStack;
  std::stack<Color> colorStack;
  std::stack<MatrixState> matrixStack;
};

inline GpuState *GpuStateManager::GetState() {
  GpuState *newState = new GpuState();
  newState->viewport = GetViewportState();
  newState->color = GetColorState();
  newState->blend = GetBlendState();
  newState->alphaTest = GetAlphaTestState();
  newState->depthTest = GetDepthTestState();
  return newState;
}

inline void GpuStateManager::PushStates(uint flags) {
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

inline void GpuStateManager::PopStates(uint flags) {
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

inline long GpuStateManager::SetViewport(int x, int y, int width,
                                            int height) {
  Viewport3D vp(x, y, width, height, 0, 0, 0);
  return SetViewportState(vp);
}

inline long GpuStateManager::SetColor(float red, float green, float blue,
                                         float alpha) {
  Color color(red, green, blue, alpha);
  return SetColorState(color);
}

inline MatrixState GpuStateManager::GetMatrixState() {
  ::base::Matrix worldview = GetWorldViewMatrix();
  ::base::Matrix projection = GetProjectionMatrix();
  return MatrixState(worldview, projection);
}

inline long GpuStateManager::SetMatrixState(MatrixState &state) {
  SetWorldViewMatrix(state.worldview);
  SetProjectionMatrix(state.projection);

  return kErrNone;
}
}  // namespace detail
}  // namespace scenic

#if !defined(SCENIC_IMPL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD3D_STATESMANAGER_H