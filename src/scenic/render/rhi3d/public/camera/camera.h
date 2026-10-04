// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _RD3D_CAMERA_H
#define _RD3D_CAMERA_H

#include <memory>

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/base.h"
#include "scenic/render/rhi3d/public/device/render_device.h"

namespace scenic {
namespace detail {
class RenderDevice3d;
typedef class RenderDevice3d* LP3DRENDERDEVICE;

// Base leftover 3D camera: owns a viewport and can push it to the device.
class LEGACY_RENDER_EXPORT Camera {
 public:
  Camera(LP3DRENDERDEVICE device, const Viewport3D& viewport);
  virtual ~Camera();

  void set_viewport(const Viewport3D& viewport) { m_viewport = viewport; }
  [[nodiscard]] const Viewport3D& viewport() const { return m_viewport; }

  virtual long apply();

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  Viewport3D m_viewport;
};

// Orthographic / screen-space projection helper for leftover HUD-style views.
class LEGACY_RENDER_EXPORT OrthCamera : public Camera {
 public:
  OrthCamera(LP3DRENDERDEVICE device, const Viewport3D& viewport);
  ~OrthCamera() override;

  void set_identity(bool value) { m_bIdentity = value; }
  [[nodiscard]] bool identity() const { return m_bIdentity; }
  void set_inverse(bool value) { m_bInverse = value; }
  [[nodiscard]] bool inverse() const { return m_bInverse; }

  long apply() override;

 protected:
  bool m_bIdentity = false;
  bool m_bInverse = false;
};

// Perspective look-at camera with walk / orbit / pitch-yaw-roll controls.
class LEGACY_RENDER_EXPORT PerspCamera : public Camera {
 public:
  PerspCamera(LP3DRENDERDEVICE device, const Viewport3D& viewport);
  ~PerspCamera() override;

  long apply() override;

  // Qualify ::base:: on exported signatures so MSVC dllimport mangling
  // matches scenic_impl exports (scenic::detail::Vector3 alias is unsafe here).
  void set_eye(const ::base::Vector3& eye) { m_vEye = eye; }
  [[nodiscard]] const ::base::Vector3& eye() const { return m_vEye; }
  // Mutable accessor for leftover callers that mutate in place.
  [[nodiscard]] ::base::Vector3& eye() { return m_vEye; }

  void set_up(const ::base::Vector3& up) { m_vUp = up; }
  [[nodiscard]] const ::base::Vector3& up() const { return m_vUp; }
  [[nodiscard]] ::base::Vector3& up() { return m_vUp; }

  void set_target(const ::base::Vector3& target) { m_vTarget = target; }
  [[nodiscard]] const ::base::Vector3& target() const { return m_vTarget; }
  [[nodiscard]] ::base::Vector3& target() { return m_vTarget; }

  void set_etu(const ::base::Vector3& eye, const ::base::Vector3& target,
               const ::base::Vector3& up);

  void set_move_step(float step) { m_fMoveStep = step; }
  [[nodiscard]] float move_step() const { return m_fMoveStep; }

  void move_eye_smoothly(bool forward = true);
  void move_eye_immediately(float distance);

  void pitch(float angle);
  void yaw(float angle);
  void roll(float angle);

  void move_forward();
  void move_back();
  void move_left();
  void move_right();
  void move_up();
  void move_down();

  // Former CombinedCamera surface (merged).
  void set_camera(const Vector3& eye, const Vector3& target, const Vector3& up);
  void raise_view_direction(float angle);
  void turn_view_direction(float angle);
  void shift_camera(float step);
  void forward_camera(float step);
  void rise_camera(float step);
  void lean_camera(float angle);
  void move_camera(const Vector3& delta);
  void move_camera_to_pos(const Vector3& pos);
  void set_sphere_camera_move(long delt_x, long delt_y);

 protected:
  Vector3 m_vEye;
  Vector3 m_vUp;
  Vector3 m_vTarget;

  float m_fMoveStep = 5.f;
  float m_fSmoothX = 0.f;
};

// First-person camera: mouse look relative to a window center.
class LEGACY_RENDER_EXPORT FpsCamera : public PerspCamera {
 public:
  FpsCamera(LP3DRENDERDEVICE device, const Viewport3D& viewport);
  ~FpsCamera() override;

  void set_win_center(lPoint center) { m_winCenter = center; }
  void set_view_by_mouse();

 private:
  lPoint m_winCenter{};
};

// Orbit / trackball camera around the look-at target.
class LEGACY_RENDER_EXPORT ArbvCamera : public PerspCamera {
 public:
  ArbvCamera(LP3DRENDERDEVICE device, const Viewport3D& viewport);
  ~ArbvCamera() override;

  void set_arbit_radius(float radius) { m_fRaduis = radius; }
  [[nodiscard]] float arbit_radius() const { return m_fRaduis; }

  void set_arbit_move(long delt_x, long delt_y);

 private:
  float m_fRaduis = 0.f;
};

enum class View3dCameraKind { kPersp, kArbv, kFps };

[[nodiscard]] LEGACY_RENDER_EXPORT std::unique_ptr<PerspCamera>
make_view3d_camera(View3dCameraKind kind, LP3DRENDERDEVICE device,
                   const Viewport3D& viewport);

}  // namespace detail
}  // namespace scenic

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  // _RD3D_CAMERA_H
