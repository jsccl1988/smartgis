// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/scene/frustum_aabb.h"

#include <cmath>

namespace render {
namespace scene {
namespace {

void mul4(const float a[16], const float b[16], float out[16]) {
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      float sum = 0.f;
      for (int k = 0; k < 4; ++k) {
        sum += a[row + k * 4] * b[k + col * 4];
      }
      out[row + col * 4] = sum;
    }
  }
}

void normalize_plane(float* p) {
  const float len =
      std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
  if (len > 1e-8f) {
    const float inv = 1.f / len;
    p[0] *= inv;
    p[1] *= inv;
    p[2] *= inv;
    p[3] *= inv;
  }
}

}  // namespace

FrustumPlanes extract_frustum_planes(const float view[16],
                                     const float proj[16]) {
  float vp[16];
  mul4(proj, view, vp);

  // Column-major clip = VP * vec → Gribb/Hartmann from matrix *rows*
  // (row i is vp[i], vp[i+4], vp[i+8], vp[i+12]). Inward normals.
  FrustumPlanes out;
  // Right:  row3 - row0
  out.planes[0][0] = vp[3] - vp[0];
  out.planes[0][1] = vp[7] - vp[4];
  out.planes[0][2] = vp[11] - vp[8];
  out.planes[0][3] = vp[15] - vp[12];
  // Left:   row3 + row0
  out.planes[1][0] = vp[3] + vp[0];
  out.planes[1][1] = vp[7] + vp[4];
  out.planes[1][2] = vp[11] + vp[8];
  out.planes[1][3] = vp[15] + vp[12];
  // Bottom: row3 + row1
  out.planes[2][0] = vp[3] + vp[1];
  out.planes[2][1] = vp[7] + vp[5];
  out.planes[2][2] = vp[11] + vp[9];
  out.planes[2][3] = vp[15] + vp[13];
  // Top:    row3 - row1
  out.planes[3][0] = vp[3] - vp[1];
  out.planes[3][1] = vp[7] - vp[5];
  out.planes[3][2] = vp[11] - vp[9];
  out.planes[3][3] = vp[15] - vp[13];
  // Far:    row3 - row2
  out.planes[4][0] = vp[3] - vp[2];
  out.planes[4][1] = vp[7] - vp[6];
  out.planes[4][2] = vp[11] - vp[10];
  out.planes[4][3] = vp[15] - vp[14];
  // Near:   row3 + row2
  out.planes[5][0] = vp[3] + vp[2];
  out.planes[5][1] = vp[7] + vp[6];
  out.planes[5][2] = vp[11] + vp[10];
  out.planes[5][3] = vp[15] + vp[14];

  for (int i = 0; i < 6; ++i) {
    normalize_plane(out.planes[i]);
  }
  return out;
}

FrustumPlanes extract_frustum_planes(
    const render::rhi::CameraMatrices& camera) {
  return extract_frustum_planes(camera.view, camera.proj);
}

bool aabb_intersects_frustum(float min_x, float min_y, float min_z,
                             float max_x, float max_y, float max_z,
                             const FrustumPlanes& frustum) {
  for (int i = 0; i < 6; ++i) {
    const float* p = frustum.planes[i];
    // Positive-vertex test (inward normals): if the AABB corner farthest
    // along the plane normal is still outside (n·x+d < 0), the box is culled.
    const float vx = p[0] >= 0.f ? max_x : min_x;
    const float vy = p[1] >= 0.f ? max_y : min_y;
    const float vz = p[2] >= 0.f ? max_z : min_z;
    if (p[0] * vx + p[1] * vy + p[2] * vz + p[3] < 0.f) {
      return false;
    }
  }
  return true;
}

}  // namespace scene
}  // namespace render
