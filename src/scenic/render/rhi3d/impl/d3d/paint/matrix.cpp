// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cmath>

#include "scenic/render/rhi3d/impl/d3d/host/render_device.h"

namespace scenic {
namespace detail {

long D3dRenderDevice::MatrixModeSet(MatrixMode mode) {
  m_matrixMode = mode;
  return SMT_ERR_NONE;
}

MatrixMode D3dRenderDevice::MatrixModeGet() const { return m_matrixMode; }

long D3dRenderDevice::MatrixLoadIdentity() {
  active_matrix().identity();
  return SMT_ERR_NONE;
}

long D3dRenderDevice::MatrixLoad(const Matrix& m) {
  active_matrix() = m;
  return SMT_ERR_NONE;
}

long D3dRenderDevice::MatrixPush() {
  if (!modelview_stack_ || !projection_stack_) {
    return SMT_ERR_FAILURE;
  }
  if (m_matrixMode == MM_PROJECTION) {
    if (projection_sp_ < 0 || projection_sp_ >= kMatrixStackMax) {
      return SMT_ERR_NONE;  // soft ignore — do not AV
    }
    projection_stack_[projection_sp_++] = projection_;
  } else {
    if (modelview_sp_ < 0 || modelview_sp_ >= kMatrixStackMax) {
      return SMT_ERR_NONE;
    }
    modelview_stack_[modelview_sp_++] = modelview_;
  }
  return SMT_ERR_NONE;
}

long D3dRenderDevice::MatrixPop() {
  if (!modelview_stack_ || !projection_stack_) {
    return SMT_ERR_FAILURE;
  }
  if (m_matrixMode == MM_PROJECTION) {
    if (projection_sp_ <= 0 || projection_sp_ > kMatrixStackMax) {
      return SMT_ERR_NONE;
    }
    projection_ = projection_stack_[--projection_sp_];
  } else {
    if (modelview_sp_ <= 0 || modelview_sp_ > kMatrixStackMax) {
      return SMT_ERR_NONE;
    }
    modelview_ = modelview_stack_[--modelview_sp_];
  }
  return SMT_ERR_NONE;
}

long D3dRenderDevice::MatrixScale(float x, float y, float z) {
  Matrix s;
  s.identity();
  s.scale(x, y, z);
  active_matrix() *= s;
  return SMT_ERR_NONE;
}

long D3dRenderDevice::MatrixTranslation(float x, float y, float z) {
  Matrix t;
  t.identity();
  t.translate(x, y, z);
  active_matrix() *= t;
  return SMT_ERR_NONE;
}

long D3dRenderDevice::MatrixRotation(float angle, float x, float y,
                                        float z) {
  // Leftover GL path used degrees; convert for Matrix::rotate_axis (radians).
  Matrix r;
  r.identity();
  const float rad = angle * (3.14159265358979323846f / 180.f);
  r.rotate(rad, x, y, z);
  active_matrix() *= r;
  return SMT_ERR_NONE;
}

long D3dRenderDevice::MatrixMultiply(const Matrix& m) {
  active_matrix() *= m;
  return SMT_ERR_NONE;
}

Matrix D3dRenderDevice::MatrixGet() { return active_matrix(); }

long D3dRenderDevice::SetOrtho(float left, float right, float bottom,
                                  float top, float zNear, float zFar) {
  Matrix& m = active_matrix();
  m.identity();
  const float rl = right - left;
  const float tb = top - bottom;
  const float fn = zFar - zNear;
  if (rl == 0.f || tb == 0.f || fn == 0.f) return SMT_ERR_FAILURE;
  m._11 = 2.f / rl;
  m._22 = 2.f / tb;
  m._33 = -2.f / fn;
  m._41 = -(right + left) / rl;
  m._42 = -(top + bottom) / tb;
  m._43 = -(zFar + zNear) / fn;
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetPerspective(float fovy, float aspect, float zNear,
                                        float zFar) {
  // RH perspective matching leftover GL (gluPerspective), Z in [-w, w].
  // Draw path remaps to D3D clip Z [0, w] via a post-multiply fix matrix.
  Matrix& m = active_matrix();
  m.identity();
  if (aspect == 0.f || zNear <= 0.f || zFar <= zNear) {
    return SMT_ERR_FAILURE;
  }
  const float f = 1.f / std::tan(fovy * (3.14159265358979323846f / 360.f));
  m._11 = f / aspect;
  m._22 = f;
  m._33 = (zFar + zNear) / (zNear - zFar);
  m._34 = -1.f;
  m._43 = (2.f * zFar * zNear) / (zNear - zFar);
  m._44 = 0.f;
  return SMT_ERR_NONE;
}

long D3dRenderDevice::SetViewLookAt(Vector3& vPos, Vector3& vView,
                                       Vector3& vUp) {
  // gluLookAt-compatible RH view (leftover camera / StereoHwnd orbit).
  Matrix& m = active_matrix();
  m.view_look_at(Vector4(vPos.x, vPos.y, vPos.z),
                 Vector4(vView.x, vView.y, vView.z),
                 Vector4(vUp.x, vUp.y, vUp.z));
  // Degenerate eye==target leaves identity; treat as failure like before.
  const float fx = vView.x - vPos.x;
  const float fy = vView.y - vPos.y;
  const float fz = vView.z - vPos.z;
  if (fx * fx + fy * fy + fz * fz < 1e-12f) {
    return SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

long D3dRenderDevice::GetFrustum(Frustum& frustum) {
  // Draw path uses row-vector clip = pos * modelview * projection (* gl_to_d3d
  // only at PS). Frustum cull must use the same MV*P (GL NDC z).
  frustum = Frustum::from_view_proj(modelview_ * projection_);
  return SMT_ERR_NONE;
}

long D3dRenderDevice::Transform2DTo3D(Vector3& /*vOrg*/, Vector3& /*vTar*/,
                                         const lPoint& /*point*/) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::Transform3DTo2D(const Vector3& ver3D, lPoint& point) {
  // Match gluProject: clip → NDC → bottom-up window Y; MapLabelBatch flips.
  // Manual clip multiply — Matrix::transform_point divides by w and forces w=1.
  // Re-apply camera view before label projection when P3 deferred may have
  // raced modelview_ (scene Render calls camera->apply() each frame first).
  const Matrix mvp = modelview_ * projection_;
  const float x = ver3D.x;
  const float y = ver3D.y;
  const float z = ver3D.z;
  const float clip_x =
      x * mvp._11 + y * mvp._21 + z * mvp._31 + mvp._41;
  const float clip_y =
      x * mvp._12 + y * mvp._22 + z * mvp._32 + mvp._42;
  const float clip_w =
      x * mvp._14 + y * mvp._24 + z * mvp._34 + mvp._44;
  if (std::fabs(clip_w) < 1e-8f || clip_w < 0.f) {
    return SMT_ERR_FAILURE;
  }
  const float ndc_x = clip_x / clip_w;
  const float ndc_y = clip_y / clip_w;
  const float vw = static_cast<float>(
      m_viewPort.ulWidth > 0 ? m_viewPort.ulWidth : backbuffer_width_);
  const float vh = static_cast<float>(
      m_viewPort.ulHeight > 0 ? m_viewPort.ulHeight : backbuffer_height_);
  if (vw <= 0.f || vh <= 0.f) {
    return SMT_ERR_FAILURE;
  }
  point.x = static_cast<long>(m_viewPort.ulX + (ndc_x + 1.f) * 0.5f * vw);
  point.y = static_cast<long>(m_viewPort.ulY + (ndc_y + 1.f) * 0.5f * vh);
  return SMT_ERR_NONE;
}

}  // namespace detail
}  // namespace scenic
