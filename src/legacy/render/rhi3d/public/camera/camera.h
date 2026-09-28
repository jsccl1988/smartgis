// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _RD3D_CAMERA_H
#define _RD3D_CAMERA_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/3drenderdevice.h"
#include "legacy/render/rhi3d/public/device/base.h"

namespace render {
class Smt3DRenderDevice;
typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

class LEGACY_RENDER_EXPORT SmtCamera {
 public:
  SmtCamera(LP3DRENDERDEVICE p3DRenderDevice, Viewport3D &viewport);
  virtual ~SmtCamera(void);

 public:
  void SetViewport(Viewport3D &viewport) { m_viewport = viewport; }

 public:
  virtual long Apply(void);

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  Viewport3D m_viewport;
};

class LEGACY_RENDER_EXPORT SmtOrthCamera : public SmtCamera {
 public:
  SmtOrthCamera(LP3DRENDERDEVICE p3DRenderDevice, Viewport3D &viewport);
  virtual ~SmtOrthCamera(void);

 public:
  inline void SetIdentity(bool value) { m_bIdentity = value; }
  inline bool GetIdentity() { return m_bIdentity; }
  inline void SetInverse(bool value) { m_bInverse = value; }
  inline bool GetInverse() { return m_bInverse; }

 public:
  virtual long Apply(void);

 protected:
  bool m_bIdentity;
  bool m_bInverse;
};

class LEGACY_RENDER_EXPORT SmtPerspCamera : public SmtCamera {
 public:
  SmtPerspCamera(LP3DRENDERDEVICE p3DRenderDevice, Viewport3D &viewport);
  virtual ~SmtPerspCamera(void);

 public:
  virtual long Apply(void);

 public:
  inline void SetEye(Vector3 &eye) { m_vEye = eye; }
  inline Vector3 &GetEye() { return m_vEye; }

  inline void SetUp(Vector3 &up) { m_vUp = up; }
  inline Vector3 &GetUp() { return m_vUp; }

  inline void SetTarget(Vector3 &target) { m_vTarget = target; }
  inline Vector3 &GetTarget() { return m_vTarget; }

  inline void SetETU(Vector3 &eye, Vector3 &target, Vector3 &up) {
    m_vEye = eye;
    m_vUp = up;
    m_vTarget = target;
  }

  inline void SetMoveStep(float fStep) { m_fMoveStep = fStep; }
  inline float GetMoveStep(void) { return m_fMoveStep; }

 public:
  void MoveEyeSmoothly(bool bForward = true);
  void MoveEyeImmediately(float fDis);

  void Pitch(float angle);
  void Yaw(float angle);
  void Roll(float angle);

  void MoveForward(void);
  void MoveBack(void);
  void MoveLeft(void);
  void MoveRight(void);
  void MoveUp(void);
  void MoveDown(void);

 protected:
  Vector3 m_vEye;
  Vector3 m_vUp;
  Vector3 m_vTarget;

  float m_fMoveStep;
  float m_fSmoothX;
};

class LEGACY_RENDER_EXPORT SmtFPSCamera : public SmtPerspCamera {
 public:
  SmtFPSCamera(LP3DRENDERDEVICE p3DRenderDevice, Viewport3D &viewport);
  virtual ~SmtFPSCamera(void);

 public:
  void SetWinCenter(lPoint center) { m_winCenter = center; }
  void SetViewByMouse(void);

 private:
  lPoint m_winCenter;
};

class LEGACY_RENDER_EXPORT SmtArbvCamera : public SmtPerspCamera {
 public:
  SmtArbvCamera(LP3DRENDERDEVICE p3DRenderDevice, Viewport3D &viewport);
  virtual ~SmtArbvCamera(void);

 public:
  inline void SetArbitRaduis(float fRaduis) { m_fRaduis = fRaduis; }
  inline float GetArbitRaduis(void) { return m_fRaduis; }

 public:
  void SetArbitMove(long deltX, long deltY);

 private:
  float m_fRaduis;
};

enum class View3dCameraKind { kPersp, kArbv, kFps };

inline SmtCamera::SmtCamera(LP3DRENDERDEVICE p3DRenderDevice,
                            Viewport3D &viewport)
    : m_p3DRenderDevice(p3DRenderDevice), m_viewport(viewport) {}

inline SmtCamera::~SmtCamera(void) {}

inline long SmtCamera::Apply(void) {
  return m_p3DRenderDevice->GetStateManager()->SetViewportState(m_viewport);
}

inline SmtOrthCamera::SmtOrthCamera(LP3DRENDERDEVICE p3DRenderDevice,
                                    Viewport3D &viewport)
    : SmtCamera(p3DRenderDevice, viewport),
      m_bIdentity(false),
      m_bInverse(false) {}

inline SmtOrthCamera::~SmtOrthCamera(void) { m_p3DRenderDevice = NULL; }

inline long SmtOrthCamera::Apply(void) {
  SmtCamera::Apply();

  m_p3DRenderDevice->MatrixModeSet(MM_PROJECTION);
  m_p3DRenderDevice->MatrixLoadIdentity();
  m_p3DRenderDevice->MatrixModeSet(MM_MODELVIEW);
  m_p3DRenderDevice->MatrixLoadIdentity();

  if (!m_bIdentity && m_bInverse) {
    m_p3DRenderDevice->MatrixScale(2.0f / m_viewport.ulWidth,
                                   -2.0f / m_viewport.ulHeight, 1.0f);
    m_p3DRenderDevice->MatrixTranslation(-(m_viewport.ulWidth / 2.0f),
                                         -(m_viewport.ulHeight / 2.0f), 0.0f);
  }

  if (m_bIdentity && m_bInverse) {
    m_p3DRenderDevice->MatrixScale(2.0, -2.0, 1.0);
    m_p3DRenderDevice->MatrixTranslation(-0.5, -0.5, 0.0);
  }

  if (!m_bIdentity && !m_bInverse) {
    m_p3DRenderDevice->MatrixScale(2.0f / m_viewport.ulWidth,
                                   2.0f / m_viewport.ulHeight, 1.0f);
  }

  return SMT_ERR_NONE;
}

inline SmtPerspCamera::SmtPerspCamera(LP3DRENDERDEVICE p3DRenderDevice,
                                      Viewport3D &viewport)
    : SmtCamera(p3DRenderDevice, viewport), m_fSmoothX(0), m_fMoveStep(5.) {
  m_vEye = Vector3(0., 0., 0.);
  m_vTarget = Vector3(0.0, 1.0, 0.5);
  m_vUp = Vector3(0., 0., 1.);
}

inline SmtPerspCamera::~SmtPerspCamera(void) { m_p3DRenderDevice = NULL; }

inline void SmtPerspCamera::MoveForward(void) {
  Vector3 vDir = m_vTarget - m_vEye;
  vDir.normalize();

  m_vEye.x += vDir.x * m_fMoveStep;
  m_vEye.z += vDir.z * m_fMoveStep;
  m_vTarget.x += vDir.x * m_fMoveStep;
  m_vTarget.z += vDir.z * m_fMoveStep;
}

inline void SmtPerspCamera::MoveBack(void) {
  Vector3 vDir = m_vTarget - m_vEye;
  vDir.normalize();

  m_vEye.x -= vDir.x * m_fMoveStep;
  m_vEye.z -= vDir.z * m_fMoveStep;
  m_vTarget.x -= vDir.x * m_fMoveStep;
  m_vTarget.z -= vDir.z * m_fMoveStep;
}

inline void SmtPerspCamera::MoveLeft(void) {
  Vector3 vCross, vDir(m_vTarget - m_vEye);
  vCross = vDir.cross(m_vUp);
  vCross.normalize();

  m_vEye.x -= vCross.x * m_fMoveStep;
  m_vEye.z -= vCross.z * m_fMoveStep;

  m_vTarget.x -= vCross.x * m_fMoveStep;
  m_vTarget.z -= vCross.z * m_fMoveStep;
}

inline void SmtPerspCamera::MoveRight(void) {
  Vector3 vCross, vDir(m_vTarget - m_vEye);
  vCross = vDir.cross(m_vUp);
  vCross.normalize();

  m_vEye.x += vCross.x * m_fMoveStep;
  m_vEye.z += vCross.z * m_fMoveStep;

  m_vTarget.x += vCross.x * m_fMoveStep;
  m_vTarget.z += vCross.z * m_fMoveStep;
}

inline void SmtPerspCamera::MoveUp(void) {
  m_vEye.y += m_vUp.y * m_fMoveStep;
  m_vTarget.y += m_vUp.y * m_fMoveStep;
}

inline void SmtPerspCamera::MoveDown(void) {
  m_vEye.y -= m_vUp.y * m_fMoveStep;
  m_vTarget.y -= m_vUp.y * m_fMoveStep;
}

inline void SmtPerspCamera::MoveEyeSmoothly(bool bForward) {
  if (bForward)
    m_fSmoothX += 5.;
  else
    m_fSmoothX -= 5.;

  double angle = 0.5 * atan(0.1 * m_fSmoothX * 20) + 0.25 * PI;
  float radius = tan(angle) + 30 * sqrt(3.0) + 0.1;

  Vector3 vDir(m_vEye - m_vTarget);
  vDir.normalize();
  m_vEye = vDir * radius + m_vTarget;
}

inline void SmtPerspCamera::MoveEyeImmediately(float fDis) {
  Vector3 vDir(m_vEye - m_vTarget);
  vDir.normalize();
  m_vEye = vDir * fDis + m_vTarget;
}

inline void SmtPerspCamera::Pitch(float angle) {
  Vector3 vCross, vDir(m_vTarget - m_vEye);
  vCross = vDir.cross(m_vUp);
  vCross.normalize();
  vDir.rotate(vCross, angle);
  m_vTarget = m_vEye + vDir;
}

inline void SmtPerspCamera::Yaw(float angle) {
  Vector3 vDir(m_vTarget - m_vEye);
  vDir.rotate(m_vUp, angle);
  m_vTarget = m_vEye + vDir;
}

inline void SmtPerspCamera::Roll(float angle) {
  Vector3 vDir = m_vTarget - m_vEye;
  vDir.normalize();
  m_vUp.rotate(vDir, angle);
}

inline long SmtPerspCamera::Apply(void) {
  if (m_viewport.ulWidth == 0 || m_viewport.ulHeight == 0) {
    return SMT_ERR_FAILURE;
  }
  if (m_viewport.fZNear <= 0.f) {
    m_viewport.fZNear = 0.1f;
  }
  if (m_viewport.fZFar <= m_viewport.fZNear) {
    m_viewport.fZFar = 1000.f;
  }
  if (m_viewport.fFovy <= 0.f) {
    m_viewport.fFovy = 45.f;
  }
  SmtCamera::Apply();
  m_p3DRenderDevice->MatrixModeSet(MM_PROJECTION);
  m_p3DRenderDevice->MatrixLoadIdentity();
  m_p3DRenderDevice->SetPerspective(
      m_viewport.fFovy,
      ((float)m_viewport.ulWidth) / ((float)m_viewport.ulHeight),
      m_viewport.fZNear, m_viewport.fZFar);
  m_p3DRenderDevice->MatrixModeSet(MM_MODELVIEW);
  m_p3DRenderDevice->MatrixLoadIdentity();
  m_p3DRenderDevice->SetViewLookAt(m_vEye, m_vTarget, m_vUp);

  return SMT_ERR_NONE;
}

inline SmtFPSCamera::SmtFPSCamera(LP3DRENDERDEVICE p3DRenderDevice,
                                  Viewport3D &viewport)
    : SmtPerspCamera(p3DRenderDevice, viewport) {}

inline SmtFPSCamera::~SmtFPSCamera(void) {}

inline void SmtFPSCamera::SetViewByMouse(void) {
  POINT mousePos;

  GetCursorPos(&mousePos);

  if ((mousePos.x == m_winCenter.x) && (mousePos.y == m_winCenter.y)) return;

  SetCursorPos(m_winCenter.x, m_winCenter.y);

  float angleY = 0.0f;
  float angleZ = 0.0f;

  angleY = (float)((m_winCenter.x - mousePos.x)) / 1000.0f;
  angleZ = (float)((m_winCenter.y - mousePos.y)) / 1000.0f;

  if (angleY > 1.0) {
    angleY = 1.0;
    return;
  }

  if (angleY < -1.0) {
    angleY = -1.0;
    return;
  }

  Pitch(angleZ);
  Yaw(angleY);
}

inline SmtArbvCamera::SmtArbvCamera(LP3DRENDERDEVICE p3DRenderDevice,
                                    Viewport3D &viewport)
    : SmtPerspCamera(p3DRenderDevice, viewport), m_fRaduis(0) {}

inline SmtArbvCamera::~SmtArbvCamera(void) {}

inline void SmtArbvCamera::SetArbitMove(long deltX, long deltY) {
  Vector3 vDir(m_vEye - m_vTarget);

  m_fRaduis = vDir.length();

  vDir.normalize();
  Vector3 u = m_vUp.cross(vDir);
  u.normalize();

  Vector3 v = vDir.cross(u);
  v.normalize();

  Vector3 m = u * deltX + v * deltY;
  double len = m.length();
  len /= 15.0;
  if (len > 0.0) {
    double x = len / m_fRaduis;
    m.normalize();
    x = -1 * x;
    m_vEye = m_vTarget + (vDir * cos(x) + m * sin(x)) * m_fRaduis;
    m_vUp = v;
  }
}

inline LEGACY_RENDER_EXPORT SmtPerspCamera *make_view3d_camera(
    View3dCameraKind kind, LP3DRENDERDEVICE device, Viewport3D &viewport) {
  if (!device) {
    return nullptr;
  }
  switch (kind) {
    case View3dCameraKind::kArbv:
      return new SmtArbvCamera(device, viewport);
    case View3dCameraKind::kFps:
      return new SmtFPSCamera(device, viewport);
    case View3dCameraKind::kPersp:
      return new SmtPerspCamera(device, viewport);
  }
  return new SmtPerspCamera(device, viewport);
}
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  // _RD3D_CAMERA_H
