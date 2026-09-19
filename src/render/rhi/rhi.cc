// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/rhi.h"

#include <cmath>

namespace render {
namespace rhi {
namespace {

void look_at(float* m, float ex, float ey, float ez, float cx, float cy,
             float cz, float ux, float uy, float uz) {
  float fx = cx - ex;
  float fy = cy - ey;
  float fz = cz - ez;
  const float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
  if (fl > 1e-8f) {
    fx /= fl;
    fy /= fl;
    fz /= fl;
  }
  float sx = fy * uz - fz * uy;
  float sy = fz * ux - fx * uz;
  float sz = fx * uy - fy * ux;
  const float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
  if (sl > 1e-8f) {
    sx /= sl;
    sy /= sl;
    sz /= sl;
  }
  const float rx = sy * fz - sz * fy;
  const float ry = sz * fx - sx * fz;
  const float rz = sx * fy - sy * fx;
  set_identity4(m);
  m[0] = sx;
  m[1] = rx;
  m[2] = -fx;
  m[4] = sy;
  m[5] = ry;
  m[6] = -fy;
  m[8] = sz;
  m[9] = rz;
  m[10] = -fz;
  m[12] = -(sx * ex + sy * ey + sz * ez);
  m[13] = -(rx * ex + ry * ey + rz * ez);
  m[14] = -(-fx * ex - fy * ey - fz * ez);
}

}  // namespace

CameraMatrices make_ortho_camera(float left, float right, float bottom,
                                 float top, float near_z, float far_z) {
  CameraMatrices camera;
  camera.kind = CameraKind::kOrtho;
  set_identity4(camera.view);
  set_identity4(camera.proj);
  const float w = right - left;
  const float h = top - bottom;
  const float d = far_z - near_z;
  if (std::fabs(w) < 1e-8f || std::fabs(h) < 1e-8f || std::fabs(d) < 1e-8f) {
    return camera;
  }
  camera.proj[0] = 2.f / w;
  camera.proj[5] = 2.f / h;
  camera.proj[10] = -2.f / d;
  camera.proj[12] = -(right + left) / w;
  camera.proj[13] = -(top + bottom) / h;
  camera.proj[14] = -(far_z + near_z) / d;
  return camera;
}

CameraMatrices make_perspective_camera(float fov_y_radians, float aspect,
                                       float near_z, float far_z) {
  CameraMatrices camera;
  camera.kind = CameraKind::kPerspective;
  look_at(camera.view, 0.f, 0.f, 3.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f);
  set_identity4(camera.proj);
  if (fov_y_radians <= 0.f || aspect <= 0.f || far_z <= near_z) {
    return camera;
  }
  // Right-handed clip to match look_at (camera looks down -Z). Left-handed
  // D3D-style (proj[11]=+1) puts orbit targets behind the near plane so
  // FlyCube terrain/ocean draws clear the swapchain but rasterize nothing.
  const float f = 1.f / std::tan(fov_y_radians * 0.5f);
  const float n = near_z;
  const float fr = far_z;
  camera.proj[0] = f / aspect;
  camera.proj[5] = f;
  camera.proj[10] = fr / (n - fr);
  camera.proj[11] = -1.f;
  camera.proj[14] = (n * fr) / (n - fr);
  camera.proj[15] = 0.f;
  return camera;
}

CameraMatrices make_orbit_camera(float yaw_radians, float pitch_radians,
                                 float distance, float fov_y_radians,
                                 float aspect, float near_z, float far_z) {
  CameraMatrices camera = make_perspective_camera(fov_y_radians, aspect, near_z,
                                                  far_z);
  camera.kind = CameraKind::kPerspective;
  const float dist = distance > 0.15f ? distance : 0.15f;
  const float cp = std::cos(pitch_radians);
  const float sp = std::sin(pitch_radians);
  const float cy = std::cos(yaw_radians);
  const float sy = std::sin(yaw_radians);
  const float ex = dist * cp * sy;
  const float ey = dist * sp;
  const float ez = dist * cp * cy;
  look_at(camera.view, ex, ey, ez, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f);
  return camera;
}


Device* create_null_device();
Device* create_gdi_device();
Device* create_gl_device();
Device* create_flycube_device(Backend backend);

Device* create_device(Backend backend) {
  switch (backend) {
    case Backend::kGdi:
      return create_gdi_device();
    case Backend::kGl:
      return create_gl_device();
    case Backend::kDx12:
    case Backend::kVulkan:
      return create_flycube_device(backend);
    case Backend::kNull:
    default:
      return create_null_device();
  }
}

Backend preferred_gpu_backend() {
#if defined(_WIN32)
  return Backend::kDx12;
#else
  return Backend::kVulkan;
#endif
}

}  // namespace rhi
}  // namespace render
