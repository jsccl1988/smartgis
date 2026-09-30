// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _RD3D_COMBINEDCAMERA_H
#define _RD3D_COMBINEDCAMERA_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/base.h"

namespace render {

class LEGACY_RENDER_EXPORT SmtCombinedCamera {
 public:
  SmtCombinedCamera();
  virtual ~SmtCombinedCamera(void);

  inline void SetEye(Vector3 &eye) { m_vEye = eye; }
  inline Vector3 &GetEye() { return m_vEye; }

  inline void SetUp(Vector3 &up) { m_vUp = up; }
  inline Vector3 &GetUp() { return m_vUp; }

  inline void SetTarget(Vector3 &target) { m_vTarget = target; }
  inline Vector3 &GetTarget() { return m_vTarget; }

  void SetCamera(Vector3 &eye, Vector3 &target, Vector3 &up);

 public:
  void RaiseViewDirection(float angle);
  void TurnViewDirection(float angle);

  void ShiftCamera(float step);
  void ForwardCamera(float step);
  void RiseCamera(float step);
  void LeanCamera(float angle);

  void MoveCamera(Vector3 &vec);
  void MoveCameraToPos(Vector3 &pos);

  void SetSphereCameraMove(long deltX, long deltY);

 private:
  Vector3 m_vEye;
  Vector3 m_vUp;
  Vector3 m_vTarget;
};

inline SmtCombinedCamera::SmtCombinedCamera() {
  Vector3 zero = Vector3(0., 0., 0.);
  Vector3 view = Vector3(0.0, 1.0, 0.5);
  Vector3 up = Vector3(0., 0., 1.);

  m_vEye = zero;
  m_vTarget = view;
  m_vUp = up;
}

inline SmtCombinedCamera::~SmtCombinedCamera(void) {}

inline void SmtCombinedCamera::SetCamera(Vector3 &pos, Vector3 &view,
                                         Vector3 &up) {
  m_vEye = pos;
  m_vUp = up;
  m_vTarget = view;
}

inline void SmtCombinedCamera::ShiftCamera(float step) {
  Vector3 vCross, vDir(m_vTarget - m_vEye);
  vCross = vDir.cross(m_vUp);
  vCross.normalize();

  m_vEye.x += vCross.x * step;
  m_vEye.z += vCross.z * step;

  m_vTarget.x += vCross.x * step;
  m_vTarget.z += vCross.z * step;
}

inline void SmtCombinedCamera::ForwardCamera(float step) {
  Vector3 vDir = m_vTarget - m_vEye;
  vDir.normalize();

  m_vEye.x += vDir.x * step;
  m_vEye.z += vDir.z * step;
  m_vTarget.x += vDir.x * step;
  m_vTarget.z += vDir.z * step;
}

inline void SmtCombinedCamera::RiseCamera(float step) {
  m_vEye.y += m_vUp.y * step;
  m_vTarget.y += m_vUp.y * step;
}

inline void SmtCombinedCamera::LeanCamera(float angle) {
  Vector3 vDir = m_vTarget - m_vEye;
  vDir.normalize();
  m_vUp.rotate(vDir, angle);
}

inline void SmtCombinedCamera::MoveCamera(Vector3 &vec) {
  m_vEye += vec;
  m_vTarget += vec;
}

inline void SmtCombinedCamera::MoveCameraToPos(Vector3 &pos) {
  Vector3 dV = pos - m_vEye;
  MoveCamera(dV);
}

inline void SmtCombinedCamera::RaiseViewDirection(float angle) {
  Vector3 vCross, vDir(m_vTarget - m_vEye);
  vCross = vDir.cross(m_vUp);
  vCross.normalize();
  vDir.rotate(vCross, angle);
  m_vTarget = m_vEye + vDir;
}

inline void SmtCombinedCamera::TurnViewDirection(float angle) {
  Vector3 vDir(m_vTarget - m_vEye);
  vDir.rotate(m_vUp, angle);
  m_vTarget = m_vEye + vDir;
}

inline void SmtCombinedCamera::SetSphereCameraMove(long deltX, long deltY) {
  Vector3 vDir(m_vEye - m_vTarget);

  float radius = vDir.length();
  vDir.normalize();
  Vector3 u = m_vUp.cross(vDir);
  u.normalize();

  Vector3 v = vDir.cross(u);
  v.normalize();

  Vector3 m = u * deltX + v * deltY;
  double len = m.length();
  len /= 15.0;
  if (len > 0.0) {
    double x = len / radius;
    m.normalize();
    x = -1 * x;
    m_vEye = m_vTarget + (vDir * cos(x) + m * sin(x)) * radius;
    m_vUp = v;
  }
}

}  // namespace render

#endif  // _RD3D_COMBINEDCAMERA_H
