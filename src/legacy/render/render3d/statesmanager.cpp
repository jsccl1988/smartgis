#include "legacy/render/render3d/statesmanager.h"

namespace render {
SmtGPUState* SmtGPUStateManager::GetState() {
  SmtGPUState* newState = new SmtGPUState();
  newState->viewport = GetViewportState();
  newState->color = GetColorState();
  newState->blend = GetBlendState();
  newState->alphaTest = GetAlphaTestState();
  newState->depthTest = GetDepthTestState();
  return newState;
}

void SmtGPUStateManager::PushStates(uint flags) {
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

void SmtGPUStateManager::PopStates(uint flags) {
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

long SmtGPUStateManager::SetViewport(int x, int y, int width, int height) {
  Viewport3D vp(x, y, width, height, 0, 0, 0);
  return SetViewportState(vp);
}

// clr
long SmtGPUStateManager::SetColor(float red, float green, float blue,
                                  float alpha) {
  SmtColor color(red, green, blue, alpha);
  return SetColorState(color);
}

// matrix
SmtMatrixState SmtGPUStateManager::GetMatrixState() {
  render::Matrix worldview = GetWorldViewMatrix();
  render::Matrix projection = GetProjectionMatrix();
  return SmtMatrixState(worldview, projection);
}

long SmtGPUStateManager::SetMatrixState(SmtMatrixState& state) {
  SetWorldViewMatrix(state.worldview);
  SetProjectionMatrix(state.projection);

  return SMT_ERR_NONE;
}
}  // namespace render
