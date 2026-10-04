#include "base/core/log.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

using namespace base;
namespace scenic {
namespace detail {
// Tranformation functions
long GlRenderDevice::MatrixModeSet(MatrixMode mode) {
  m_matrixMode = mode;

  if (m_matrixMode == MM_PROJECTION)
    glMatrixMode(GL_PROJECTION);
  else
    glMatrixMode(GL_MODELVIEW);

  return SMT_ERR_NONE;
}

MatrixMode GlRenderDevice::MatrixModeGet() const { return m_matrixMode; }

long GlRenderDevice::MatrixLoadIdentity() {
  glLoadIdentity();
  return SMT_ERR_NONE;
}

long GlRenderDevice::MatrixLoad(const Matrix& mtx) {
  glLoadMatrixf((float*)(&mtx));
  return SMT_ERR_NONE;
}

long GlRenderDevice::MatrixPush() {
  glPushMatrix();
  return SMT_ERR_NONE;
}

long GlRenderDevice::MatrixPop() {
  glPopMatrix();
  return SMT_ERR_NONE;
}

long GlRenderDevice::MatrixScale(float x, float y, float z) {
  glScalef(x, y, z);
  return SMT_ERR_NONE;
}

long GlRenderDevice::MatrixTranslation(float x, float y, float z) {
  glTranslatef(x, y, z);
  return SMT_ERR_NONE;
}

long GlRenderDevice::MatrixRotation(float angle, float x, float y, float z) {
  glRotatef(angle, x, y, z);

  return SMT_ERR_NONE;
}

long GlRenderDevice::MatrixMultiply(const Matrix& mtx) {
  glMultMatrixf((float*)(&mtx));

  return SMT_ERR_NONE;
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