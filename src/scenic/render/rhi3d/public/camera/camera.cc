// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/public/camera/camera.h"

#include <algorithm>
#include <cmath>

#include <windows.h>

#include "base/math/math.h"

namespace scenic {
namespace detail {

void translate_xz(Vector3* eye, Vector3* target, const Vector3& dir,
                  float step) {
  eye->x += dir.x * step;
  eye->z += dir.z * step;
  target->x += dir.x * step;
  target->z += dir.z * step;
}

void orbit_eye_around_target(Vector3* eye, Vector3* up, const Vector3& target,
                             long delt_x, long delt_y, float* radius_out) {
  Vector3 v_dir(*eye - target);
  const float radius = v_dir.length();
  if (radius_out) {
    *radius_out = radius;
  }
  if (radius <= 0.f) {
    return;
  }
  v_dir.normalize();
  Vector3 u = up->cross(v_dir);
  u.normalize();
  Vector3 v = v_dir.cross(u);
  v.normalize();

  Vector3 m = u * static_cast<float>(delt_x) + v * static_cast<float>(delt_y);
  double len = m.length();
  len /= 15.0;
  if (len <= 0.0) {
    return;
  }
  double x = len / static_cast<double>(radius);
  m.normalize();
  x = -x;
  *eye = target + (v_dir * static_cast<float>(std::cos(x)) +
                   m * static_cast<float>(std::sin(x))) *
                      radius;
  *up = v;
}

// Camera types live in scenic::detail (see camera.h). Keep helpers above in
// the same namespace — do not close detail here.

Camera::Camera(LP3DRENDERDEVICE device, const Viewport3D& viewport)
    : m_p3DRenderDevice(device), m_viewport(viewport) {}

Camera::~Camera() = default;

long Camera::apply() {
  if (!m_p3DRenderDevice) {
    return kErrFailure;
  }
  return m_p3DRenderDevice->GetStateManager()->SetViewportState(m_viewport);
}

OrthCamera::OrthCamera(LP3DRENDERDEVICE device,
                             const Viewport3D& viewport)
    : Camera(device, viewport) {}

OrthCamera::~OrthCamera() = default;

long OrthCamera::apply() {
  Camera::apply();
  if (!m_p3DRenderDevice) {
    return kErrFailure;
  }

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

  return kErrNone;
}

PerspCamera::PerspCamera(LP3DRENDERDEVICE device,
                               const Viewport3D& viewport)
    : Camera(device, viewport),
      m_vEye(0.f, 0.f, 0.f),
      m_vUp(0.f, 0.f, 1.f),
      m_vTarget(0.f, 1.f, 0.5f) {}

PerspCamera::~PerspCamera() = default;

void PerspCamera::set_etu(const ::base::Vector3& eye,
                             const ::base::Vector3& target,
                             const ::base::Vector3& up) {
  m_vEye = eye;
  m_vTarget = target;
  m_vUp = up;
}

void PerspCamera::move_forward() {
  Vector3 v_dir = m_vTarget - m_vEye;
  v_dir.normalize();
  detail::translate_xz(&m_vEye, &m_vTarget, v_dir, m_fMoveStep);
}

void PerspCamera::move_back() {
  Vector3 v_dir = m_vTarget - m_vEye;
  v_dir.normalize();
  detail::translate_xz(&m_vEye, &m_vTarget, v_dir, -m_fMoveStep);
}

void PerspCamera::move_left() {
  Vector3 v_dir(m_vTarget - m_vEye);
  Vector3 v_cross = v_dir.cross(m_vUp);
  v_cross.normalize();
  detail::translate_xz(&m_vEye, &m_vTarget, v_cross, -m_fMoveStep);
}

void PerspCamera::move_right() {
  Vector3 v_dir(m_vTarget - m_vEye);
  Vector3 v_cross = v_dir.cross(m_vUp);
  v_cross.normalize();
  detail::translate_xz(&m_vEye, &m_vTarget, v_cross, m_fMoveStep);
}

void PerspCamera::move_up() {
  m_vEye.y += m_vUp.y * m_fMoveStep;
  m_vTarget.y += m_vUp.y * m_fMoveStep;
}

void PerspCamera::move_down() {
  m_vEye.y -= m_vUp.y * m_fMoveStep;
  m_vTarget.y -= m_vUp.y * m_fMoveStep;
}

void PerspCamera::move_eye_smoothly(bool forward) {
  if (forward) {
    m_fSmoothX += 5.f;
  } else {
    m_fSmoothX -= 5.f;
  }

  const double angle =
      0.5 * std::atan(0.1 * static_cast<double>(m_fSmoothX) * 20.0) +
      0.25 * PI;
  const float radius =
      static_cast<float>(std::tan(angle) + 30.0 * std::sqrt(3.0) + 0.1);

  Vector3 v_dir(m_vEye - m_vTarget);
  v_dir.normalize();
  m_vEye = v_dir * radius + m_vTarget;
}

void PerspCamera::move_eye_immediately(float distance) {
  Vector3 v_dir(m_vEye - m_vTarget);
  v_dir.normalize();
  m_vEye = v_dir * distance + m_vTarget;
}

void PerspCamera::pitch(float angle) {
  Vector3 v_dir(m_vTarget - m_vEye);
  Vector3 v_cross = v_dir.cross(m_vUp);
  v_cross.normalize();
  v_dir.rotate(v_cross, angle);
  m_vTarget = m_vEye + v_dir;
}

void PerspCamera::yaw(float angle) {
  Vector3 v_dir(m_vTarget - m_vEye);
  v_dir.rotate(m_vUp, angle);
  m_vTarget = m_vEye + v_dir;
}

void PerspCamera::roll(float angle) {
  Vector3 v_dir = m_vTarget - m_vEye;
  v_dir.normalize();
  m_vUp.rotate(v_dir, angle);
}

long PerspCamera::apply() {
  if (!m_p3DRenderDevice) {
    return kErrFailure;
  }
  if (m_viewport.ulWidth == 0 || m_viewport.ulHeight == 0) {
    return kErrFailure;
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
  Camera::apply();
  m_p3DRenderDevice->MatrixModeSet(MM_PROJECTION);
  m_p3DRenderDevice->MatrixLoadIdentity();
  m_p3DRenderDevice->SetPerspective(
      m_viewport.fFovy,
      static_cast<float>(m_viewport.ulWidth) /
          static_cast<float>(m_viewport.ulHeight),
      m_viewport.fZNear, m_viewport.fZFar);
  m_p3DRenderDevice->MatrixModeSet(MM_MODELVIEW);
  m_p3DRenderDevice->MatrixLoadIdentity();
  m_p3DRenderDevice->SetViewLookAt(m_vEye, m_vTarget, m_vUp);
  return kErrNone;
}

void PerspCamera::set_camera(const Vector3& eye, const Vector3& target,
                                const Vector3& up) {
  set_etu(eye, target, up);
}

void PerspCamera::shift_camera(float step) {
  Vector3 v_dir(m_vTarget - m_vEye);
  Vector3 v_cross = v_dir.cross(m_vUp);
  v_cross.normalize();
  detail::translate_xz(&m_vEye, &m_vTarget, v_cross, step);
}

void PerspCamera::forward_camera(float step) {
  Vector3 v_dir = m_vTarget - m_vEye;
  v_dir.normalize();
  detail::translate_xz(&m_vEye, &m_vTarget, v_dir, step);
}

void PerspCamera::rise_camera(float step) {
  m_vEye.y += m_vUp.y * step;
  m_vTarget.y += m_vUp.y * step;
}

void PerspCamera::lean_camera(float angle) { roll(angle); }

void PerspCamera::move_camera(const Vector3& delta) {
  m_vEye += delta;
  m_vTarget += delta;
}

void PerspCamera::move_camera_to_pos(const Vector3& pos) {
  move_camera(pos - m_vEye);
}

void PerspCamera::raise_view_direction(float angle) { pitch(angle); }

void PerspCamera::turn_view_direction(float angle) { yaw(angle); }

void PerspCamera::set_sphere_camera_move(long delt_x, long delt_y) {
  detail::orbit_eye_around_target(&m_vEye, &m_vUp, m_vTarget, delt_x, delt_y,
                                  nullptr);
}

FpsCamera::FpsCamera(LP3DRENDERDEVICE device, const Viewport3D& viewport)
    : PerspCamera(device, viewport) {}

FpsCamera::~FpsCamera() = default;

void FpsCamera::set_view_by_mouse() {
  POINT mouse_pos{};
  GetCursorPos(&mouse_pos);

  if (mouse_pos.x == m_winCenter.x && mouse_pos.y == m_winCenter.y) {
    return;
  }

  SetCursorPos(m_winCenter.x, m_winCenter.y);

  float angle_y =
      static_cast<float>(m_winCenter.x - mouse_pos.x) / 1000.0f;
  float angle_z =
      static_cast<float>(m_winCenter.y - mouse_pos.y) / 1000.0f;

  if (angle_y > 1.0f || angle_y < -1.0f) {
    angle_y = std::clamp(angle_y, -1.0f, 1.0f);
    return;
  }

  pitch(angle_z);
  yaw(angle_y);
}

ArbvCamera::ArbvCamera(LP3DRENDERDEVICE device,
                             const Viewport3D& viewport)
    : PerspCamera(device, viewport) {}

ArbvCamera::~ArbvCamera() = default;

void ArbvCamera::set_arbit_move(long delt_x, long delt_y) {
  orbit_eye_around_target(&m_vEye, &m_vUp, m_vTarget, delt_x, delt_y,
                          &m_fRaduis);
}

std::unique_ptr<PerspCamera> make_view3d_camera(View3dCameraKind kind,
                                                   LP3DRENDERDEVICE device,
                                                   const Viewport3D& viewport) {
  if (!device) {
    return nullptr;
  }
  switch (kind) {
    case View3dCameraKind::kArbv:
      return std::make_unique<ArbvCamera>(device, viewport);
    case View3dCameraKind::kFps:
      return std::make_unique<FpsCamera>(device, viewport);
    case View3dCameraKind::kPersp:
      return std::make_unique<PerspCamera>(device, viewport);
    default:
      return std::make_unique<PerspCamera>(device, viewport);
  }
}

}  // namespace detail
}  // namespace scenic
