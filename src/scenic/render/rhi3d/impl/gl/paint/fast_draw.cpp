// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

namespace scenic {
namespace detail {
// print implement
long GlRenderDevice::DrawCube3D(Vector3 vCenter, float fWidth, Color color) {
  float width = fWidth / 2.;
  Vector3 vTopLeftFront(vCenter.x - width, vCenter.y + width,
                        vCenter.z + width);
  Vector3 vTopLeftBack(vCenter.x - width, vCenter.y + width, vCenter.z - width);
  Vector3 vTopRightBack(vCenter.x + width, vCenter.y + width,
                        vCenter.z - width);
  Vector3 vTopRightFront(vCenter.x + width, vCenter.y + width,
                         vCenter.z + width);

  Vector3 vBottomLeftFront(vCenter.x - width, vCenter.y - width,
                           vCenter.z + width);
  Vector3 vBottomLeftBack(vCenter.x - width, vCenter.y - width,
                          vCenter.z - width);
  Vector3 vBottomRightBack(vCenter.x + width, vCenter.y - width,
                           vCenter.z - width);
  Vector3 vBottomRightFront(vCenter.x + width, vCenter.y - width,
                            vCenter.z + width);

  const GLboolean lighting = glIsEnabled(GL_LIGHTING);
  glDisable(GL_LIGHTING);
  glColor4f(color.fRed, color.fGreen, color.fBlue, color.fA);
  glBegin(GL_LINES);
  ////////// TOP LINES //////////
  // Store the top front line of the box
  glVertex3f(vTopLeftFront.x, vTopLeftFront.y, vTopLeftFront.z);
  glVertex3f(vTopRightFront.x, vTopRightFront.y, vTopRightFront.z);

  // Store the top back line of the box
  glVertex3f(vTopLeftBack.x, vTopLeftBack.y, vTopLeftBack.z);
  glVertex3f(vTopRightBack.x, vTopRightBack.y, vTopRightBack.z);

  // Store the top left line of the box
  glVertex3f(vTopLeftFront.x, vTopLeftFront.y, vTopLeftFront.z);
  glVertex3f(vTopLeftBack.x, vTopLeftBack.y, vTopLeftBack.z);

  // Store the top right line of the box
  glVertex3f(vTopRightFront.x, vTopRightFront.y, vTopRightFront.z);
  glVertex3f(vTopRightBack.x, vTopRightBack.y, vTopRightBack.z);

  ////////// BOTTOM LINES //////////
  // Store the bottom front line of the box
  glVertex3f(vBottomLeftFront.x, vBottomLeftFront.y, vBottomLeftFront.z);
  glVertex3f(vBottomRightFront.x, vBottomRightFront.y, vBottomRightFront.z);

  // Store the bottom back line of the box
  glVertex3f(vBottomLeftBack.x, vBottomLeftBack.y, vBottomLeftBack.z);
  glVertex3f(vBottomRightBack.x, vBottomRightBack.y, vBottomRightBack.z);

  // Store the bottom left line of the box
  glVertex3f(vBottomLeftFront.x, vBottomLeftFront.y, vBottomLeftFront.z);
  glVertex3f(vBottomLeftBack.x, vBottomLeftBack.y, vBottomLeftBack.z);

  // Store the bottom right line of the box
  glVertex3f(vBottomRightFront.x, vBottomRightFront.y, vBottomRightFront.z);
  glVertex3f(vBottomRightBack.x, vBottomRightBack.y, vBottomRightBack.z);

  ////////// SIDE LINES //////////
  // Store the bottom front line of the box
  glVertex3f(vTopLeftFront.x, vTopLeftFront.y, vTopLeftFront.z);
  glVertex3f(vBottomLeftFront.x, vBottomLeftFront.y, vBottomLeftFront.z);

  // Store the back left line of the box
  glVertex3f(vTopLeftBack.x, vTopLeftBack.y, vTopLeftBack.z);
  glVertex3f(vBottomLeftBack.x, vBottomLeftBack.y, vBottomLeftBack.z);

  // Store the front right line of the box
  glVertex3f(vTopRightBack.x, vTopRightBack.y, vTopRightBack.z);
  glVertex3f(vBottomRightBack.x, vBottomRightBack.y, vBottomRightBack.z);

  // Store the front left line of the box
  glVertex3f(vTopRightFront.x, vTopRightFront.y, vTopRightFront.z);
  glVertex3f(vBottomRightFront.x, vBottomRightFront.y, vBottomRightFront.z);

  glEnd();
  if (lighting) {
    glEnable(GL_LIGHTING);
  }

  return kErrNone;
}
}  // namespace detail
}  // namespace scenic
