// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/core/log.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

namespace scenic {
namespace detail {
// Tranformation functions
long GlRenderDevice::MatrixModeSet(MatrixMode mode) {
  m_matrixMode = mode;

  if (m_matrixMode == MM_PROJECTION)
    glMatrixMode(GL_PROJECTION);
  else
    glMatrixMode(GL_MODELVIEW);

  return kErrNone;
}

MatrixMode GlRenderDevice::MatrixModeGet() const { return m_matrixMode; }

long GlRenderDevice::MatrixLoadIdentity() {
  glLoadIdentity();
  return kErrNone;
}

long GlRenderDevice::MatrixLoad(const Matrix& mtx) {
  glLoadMatrixf((float*)(&mtx));
  return kErrNone;
}

long GlRenderDevice::MatrixPush() {
  glPushMatrix();
  return kErrNone;
}

long GlRenderDevice::MatrixPop() {
  glPopMatrix();
  return kErrNone;
}

long GlRenderDevice::MatrixScale(float x, float y, float z) {
  glScalef(x, y, z);
  return kErrNone;
}

long GlRenderDevice::MatrixTranslation(float x, float y, float z) {
  glTranslatef(x, y, z);
  return kErrNone;
}

long GlRenderDevice::MatrixRotation(float angle, float x, float y, float z) {
  glRotatef(angle, x, y, z);

  return kErrNone;
}

long GlRenderDevice::MatrixMultiply(const Matrix& mtx) {
  glMultMatrixf((float*)(&mtx));

  return kErrNone;
}

Matrix GlRenderDevice::MatrixGet() {
  Matrix mtxTmp;

  if (m_matrixMode == MM_PROJECTION)
    glGetFloatv(GL_PROJECTION_MATRIX, (float*)&mtxTmp);
  else
    glGetFloatv(GL_MODELVIEW_MATRIX, (float*)&mtxTmp);

  return mtxTmp;
}
}  // namespace detail
}  // namespace scenic