// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/cull/frustum_aabb.h"

#include <cmath>

namespace vista {
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

bool invert4(const float m[16], float inv[16]) {
  // Column-major 4x4 inverse (MESA / gluInvertMatrix).
  const float inv0 =
      m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
      m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  const float inv4 =
      -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
      m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  const float inv8 =
      m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
      m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  const float inv12 =
      -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
      m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  const float inv1 =
      -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
      m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  const float inv5 =
      m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
      m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  const float inv9 =
      -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
      m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  const float inv13 =
      m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
      m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  const float inv2 =
      m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
      m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  const float inv6 =
      -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
      m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  const float inv10 =
      m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
      m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  const float inv14 =
      -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
      m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  const float inv3 =
      -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
      m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  const float inv7 =
      m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
      m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  const float inv11 =
      -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
      m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  const float inv15 =
      m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
      m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

  const float det = m[0] * inv0 + m[1] * inv4 + m[2] * inv8 + m[3] * inv12;
  if (std::fabs(det) < 1e-12f) {
    return false;
  }
  const float inv_det = 1.f / det;
  inv[0] = inv0 * inv_det;
  inv[1] = inv1 * inv_det;
  inv[2] = inv2 * inv_det;
  inv[3] = inv3 * inv_det;
  inv[4] = inv4 * inv_det;
  inv[5] = inv5 * inv_det;
  inv[6] = inv6 * inv_det;
  inv[7] = inv7 * inv_det;
  inv[8] = inv8 * inv_det;
  inv[9] = inv9 * inv_det;
  inv[10] = inv10 * inv_det;
  inv[11] = inv11 * inv_det;
  inv[12] = inv12 * inv_det;
  inv[13] = inv13 * inv_det;
  inv[14] = inv14 * inv_det;
  inv[15] = inv15 * inv_det;
  return true;
}

void transform_clip(const float inv_vp[16], float x, float y, float z,
                    float* ox, float* oy, float* oz) {
  const float iw =
      inv_vp[3] * x + inv_vp[7] * y + inv_vp[11] * z + inv_vp[15];
  const float w = (std::fabs(iw) > 1e-12f) ? (1.f / iw) : 0.f;
  *ox = (inv_vp[0] * x + inv_vp[4] * y + inv_vp[8] * z + inv_vp[12]) * w;
  *oy = (inv_vp[1] * x + inv_vp[5] * y + inv_vp[9] * z + inv_vp[13]) * w;
  *oz = (inv_vp[2] * x + inv_vp[6] * y + inv_vp[10] * z + inv_vp[14]) * w;
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

bool frustum_world_aabb(const float view[16], const float proj[16],
                        float* min_x, float* min_y, float* min_z, float* max_x,
                        float* max_y, float* max_z) {
  if (!min_x || !min_y || !min_z || !max_x || !max_y || !max_z) {
    return false;
  }
  float vp[16];
  mul4(proj, view, vp);
  float inv_vp[16];
  if (!invert4(vp, inv_vp)) {
    return false;
  }
  bool first = true;
  const float ndc[2] = {-1.f, 1.f};
  for (int ix = 0; ix < 2; ++ix) {
    for (int iy = 0; iy < 2; ++iy) {
      for (int iz = 0; iz < 2; ++iz) {
        float wx = 0.f;
        float wy = 0.f;
        float wz = 0.f;
        transform_clip(inv_vp, ndc[ix], ndc[iy], ndc[iz], &wx, &wy, &wz);
        if (first) {
          *min_x = *max_x = wx;
          *min_y = *max_y = wy;
          *min_z = *max_z = wz;
          first = false;
        } else {
          if (wx < *min_x) {
            *min_x = wx;
          }
          if (wy < *min_y) {
            *min_y = wy;
          }
          if (wz < *min_z) {
            *min_z = wz;
          }
          if (wx > *max_x) {
            *max_x = wx;
          }
          if (wy > *max_y) {
            *max_y = wy;
          }
          if (wz > *max_z) {
            *max_z = wz;
          }
        }
      }
    }
  }
  return true;
}

}  // namespace vista
