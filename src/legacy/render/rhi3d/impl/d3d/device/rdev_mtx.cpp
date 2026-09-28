// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/device/3drenderdevice.h"

namespace render {

long SmtD3DRenderDevice::MatrixModeSet(MatrixMode mode) {
  m_matrixMode = mode;
  return SMT_ERR_NONE;
}

MatrixMode SmtD3DRenderDevice::MatrixModeGet() const { return m_matrixMode; }

long SmtD3DRenderDevice::MatrixLoadIdentity() {
  active_matrix().identity();
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixLoad(const Matrix& m) {
  active_matrix() = m;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixPush() {
  if (m_matrixMode == MM_PROJECTION)
    projection_stack_.push(projection_);
  else
    modelview_stack_.push(modelview_);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixPop() {
  if (m_matrixMode == MM_PROJECTION) {
    if (projection_stack_.empty()) return SMT_ERR_FAILURE;
    projection_ = projection_stack_.top();
    projection_stack_.pop();
  } else {
    if (modelview_stack_.empty()) return SMT_ERR_FAILURE;
    modelview_ = modelview_stack_.top();
    modelview_stack_.pop();
  }
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixScale(float x, float y, float z) {
  Matrix s;
  s.identity();
  s.scale(x, y, z);
  active_matrix() *= s;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixTranslation(float x, float y, float z) {
  Matrix t;
  t.identity();
  t.translate(x, y, z);
  active_matrix() *= t;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixRotation(float angle, float x, float y,
                                        float z) {
  // Leftover GL path used degrees; convert for Matrix::rotate_axis (radians).
  Matrix r;
  r.identity();
  const float rad = angle * (3.14159265358979323846f / 180.f);
  r.rotate(rad, x, y, z);
  active_matrix() *= r;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixMultiply(const Matrix& m) {
  active_matrix() *= m;
  return SMT_ERR_NONE;
}

Matrix SmtD3DRenderDevice::MatrixGet() { return active_matrix(); }

long SmtD3DRenderDevice::SetOrtho(float left, float right, float bottom,
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

long SmtD3DRenderDevice::SetPerspective(float fovy, float aspect, float zNear,
                                        float zFar) {
  active_matrix().identity();
  active_matrix().set_perspective(fovy, aspect, zNear, zFar);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetViewLookAt(Vector3& vPos, Vector3& vView,
                                       Vector3& vUp) {
  active_matrix().look_at(Vector4(vPos.x, vPos.y, vPos.z),
                          Vector4(vView.x, vView.y, vView.z),
                          Vector4(vUp.x, vUp.y, vUp.z));
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::GetFrustum(SmtFrustum& /*frustum*/) {
  // Plane extract from clip matrix deferred (no draw consumers in v1).
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::Transform2DTo3D(Vector3& /*vOrg*/, Vector3& /*vTar*/,
                                         const lPoint& /*point*/) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::Transform3DTo2D(const Vector3& /*ver3D*/,
                                         lPoint& /*point*/) {
  return SMT_ERR_FAILURE;
}

}  // namespace render
