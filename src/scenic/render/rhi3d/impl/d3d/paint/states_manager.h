// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_STATESMANAGER_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_STATESMANAGER_H_

#include "scenic/render/rhi3d/impl/d3d/prerequisites.h"
#include "scenic/render/rhi3d/public/state/states_manager.h"

namespace scenic {
namespace detail {

// CPU-side pipeline state cache for the leftover D3D11 device (v1).
class D3dGpuStateManager : public GpuStateManager {
 public:
  D3dGpuStateManager()
      : clear_depth_(1.f),
        clear_stencil_(0),
        line_width_(1.f),
        point_size_(1.f) {
    clear_color_[0] = clear_color_[1] = clear_color_[2] = 0.f;
    clear_color_[3] = 1.f;
    world_view_.identity();
    projection_.identity();
  }

  AlphaTestState GetAlphaTestState(void) override { return alpha_; }
  long SetAlphaTestState(AlphaTestState& state) override {
    alpha_ = state;
    return kErrNone;
  }
  long SetAlphaTest(bool enabled) override {
    alpha_.bEnabled = enabled;
    return kErrNone;
  }
  long SetAlphaTestFunc(Comparison func, float ref) override {
    alpha_.cmpFunc = func;
    alpha_.fRefValue = ref;
    return kErrNone;
  }

  DepthTestState GetDepthTestState(void) override { return depth_; }
  long SetDepthTestState(DepthTestState& state) override {
    depth_ = state;
    return kErrNone;
  }
  long SetDepthTest(bool enabled) override {
    depth_.bEnabled = enabled;
    return kErrNone;
  }
  long SetDepthTestFunc(Comparison func) override {
    depth_.cmpFunc = func;
    return kErrNone;
  }

  BlendState GetBlendState(void) override { return blend_; }
  long SetBlendState(BlendState& state) override {
    blend_ = state;
    return kErrNone;
  }
  long SetBlending(bool enabled) override {
    blend_.bEnabled = enabled;
    return kErrNone;
  }

  Viewport3D GetViewportState(void) override { return viewport_; }
  long SetViewportState(Viewport3D& vp) override {
    viewport_ = vp;
    return kErrNone;
  }

  Color GetColorState(void) override { return color_; }
  long SetColorState(Color& colorState) override {
    color_ = colorState;
    return kErrNone;
  }

  long Set2DTextures(bool /*enabled*/) override { return kErrNone; }
  long Set2DRectTextures(bool /*enabled*/) override { return kErrNone; }
  long SetSampler(TextureSampler& /*sampler*/) override {
    return kErrNone;
  }
  long SetRectSampler(TextureSampler& /*sampler*/) override {
    return kErrNone;
  }
  long SetTextureEnvironment(TextureEnvMode& /*envMode*/) override {
    return kErrNone;
  }

  Matrix GetWorldViewMatrix(void) override { return world_view_; }
  Matrix GetProjectionMatrix(void) override { return projection_; }
  long SetWorldViewMatrix(Matrix& matrix) override {
    world_view_ = matrix;
    return kErrNone;
  }
  long SetProjectionMatrix(Matrix& matrix) override {
    projection_ = matrix;
    return kErrNone;
  }

  long GetClearColorValue(float& red, float& green, float& blue,
                          float& alpha) override {
    red = clear_color_[0];
    green = clear_color_[1];
    blue = clear_color_[2];
    alpha = clear_color_[3];
    return kErrNone;
  }
  long SetClearColorValue(float red, float green, float blue,
                          float alpha = 1.f) override {
    clear_color_[0] = red;
    clear_color_[1] = green;
    clear_color_[2] = blue;
    clear_color_[3] = alpha;
    return kErrNone;
  }
  long GetClearDepthValue(float& depth) override {
    depth = clear_depth_;
    return kErrNone;
  }
  long SetClearDepthValue(float depth) override {
    clear_depth_ = depth;
    return kErrNone;
  }
  long GetStencilClearValue(ulong& s) override {
    s = clear_stencil_;
    return kErrNone;
  }
  long SetStencilClearValue(ulong s) override {
    clear_stencil_ = s;
    return kErrNone;
  }

  long SetPolygonMode(FaceMode /*face*/, PolygonMode /*mode*/) override {
    return kErrNone;
  }
  long GetLineWidth(float& size) override {
    size = line_width_;
    return kErrNone;
  }
  long SetLineWidth(float size) override {
    line_width_ = size;
    return kErrNone;
  }
  long GetPointSize(float& size) override {
    size = point_size_;
    return kErrNone;
  }
  long SetPointSize(float size) override {
    point_size_ = size;
    return kErrNone;
  }
  long EnableDepthOffset(PolygonMode /*mode*/, bool /*enabled*/) override {
    return kErrNone;
  }
  long DepthOffsetParams(float /*rFactor*/, float /*dFactor*/) override {
    return kErrNone;
  }
  long SetMaterail(bool enabled) override {
    color_material_enabled_ = enabled;
    return kErrNone;
  }
  long SetLight(bool enabled) override {
    lighting_enabled_ = enabled;
    return kErrNone;
  }

  bool lighting_enabled() const { return lighting_enabled_; }
  bool color_material_enabled() const { return color_material_enabled_; }

 private:
  AlphaTestState alpha_;
  DepthTestState depth_;
  BlendState blend_;
  Viewport3D viewport_;
  Color color_;
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

}  // namespace detail
}  // namespace scenic

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_STATESMANAGER_H_
