// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/device/statesmanager.h"

#include "legacy/render/rhi3d/impl/d3d/prerequisites.h"

namespace render {

SmtD3DGPUStateManager::SmtD3DGPUStateManager()
    : clear_depth_(1.f), clear_stencil_(0), line_width_(1.f), point_size_(1.f) {
  clear_color_[0] = clear_color_[1] = clear_color_[2] = 0.f;
  clear_color_[3] = 1.f;
  world_view_.identity();
  projection_.identity();
}

SmtAlphaTestState SmtD3DGPUStateManager::GetAlphaTestState(void) {
  return alpha_;
}

long SmtD3DGPUStateManager::SetAlphaTestState(SmtAlphaTestState& state) {
  alpha_ = state;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetAlphaTest(bool enabled) {
  alpha_.bEnabled = enabled;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetAlphaTestFunc(Comparison func, float ref) {
  alpha_.cmpFunc = func;
  alpha_.fRefValue = ref;
  return SMT_ERR_NONE;
}

SmtDepthTestState SmtD3DGPUStateManager::GetDepthTestState(void) {
  return depth_;
}

long SmtD3DGPUStateManager::SetDepthTestState(SmtDepthTestState& state) {
  depth_ = state;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetDepthTest(bool enabled) {
  depth_.bEnabled = enabled;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetDepthTestFunc(Comparison func) {
  depth_.cmpFunc = func;
  return SMT_ERR_NONE;
}

SmtBlendState SmtD3DGPUStateManager::GetBlendState(void) { return blend_; }

long SmtD3DGPUStateManager::SetBlendState(SmtBlendState& state) {
  blend_ = state;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetBlending(bool enabled) {
  blend_.bEnabled = enabled;
  return SMT_ERR_NONE;
}

Viewport3D SmtD3DGPUStateManager::GetViewportState(void) { return viewport_; }

long SmtD3DGPUStateManager::SetViewportState(Viewport3D& vp) {
  viewport_ = vp;
  return SMT_ERR_NONE;
}

SmtColor SmtD3DGPUStateManager::GetColorState(void) { return color_; }

long SmtD3DGPUStateManager::SetColorState(SmtColor& colorState) {
  color_ = colorState;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::Set2DTextures(bool /*enabled*/) {
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::Set2DRectTextures(bool /*enabled*/) {
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetSampler(TextureSampler& /*sampler*/) {
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetRectSampler(TextureSampler& /*sampler*/) {
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetTextureEnvironment(TextureEnvMode& /*envMode*/) {
  return SMT_ERR_NONE;
}

Matrix SmtD3DGPUStateManager::GetWorldViewMatrix(void) { return world_view_; }

Matrix SmtD3DGPUStateManager::GetProjectionMatrix(void) { return projection_; }

long SmtD3DGPUStateManager::SetWorldViewMatrix(Matrix& matrix) {
  world_view_ = matrix;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetProjectionMatrix(Matrix& matrix) {
  projection_ = matrix;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::GetClearColorValue(float& red, float& green,
                                               float& blue, float& alpha) {
  red = clear_color_[0];
  green = clear_color_[1];
  blue = clear_color_[2];
  alpha = clear_color_[3];
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetClearColorValue(float red, float green,
                                               float blue, float alpha) {
  clear_color_[0] = red;
  clear_color_[1] = green;
  clear_color_[2] = blue;
  clear_color_[3] = alpha;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::GetClearDepthValue(float& depth) {
  depth = clear_depth_;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetClearDepthValue(float depth) {
  clear_depth_ = depth;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::GetStencilClearValue(ulong& s) {
  s = clear_stencil_;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetStencilClearValue(ulong s) {
  clear_stencil_ = s;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetPolygonMode(FaceMode /*face*/,
                                           PolygonMode /*mode*/) {
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::GetLineWidth(float& size) {
  size = line_width_;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetLineWidth(float size) {
  line_width_ = size;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::GetPointSize(float& size) {
  size = point_size_;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetPointSize(float size) {
  point_size_ = size;
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::EnableDepthOffset(PolygonMode /*mode*/,
                                              bool /*enabled*/) {
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::DepthOffsetParams(float /*rFactor*/,
                                              float /*dFactor*/) {
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetMaterail(bool /*enabled*/) {
  return SMT_ERR_NONE;
}

long SmtD3DGPUStateManager::SetLight(bool /*enabled*/) { return SMT_ERR_NONE; }

}  // namespace render
