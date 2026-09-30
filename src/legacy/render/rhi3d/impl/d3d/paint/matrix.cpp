// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cmath>

#include "legacy/render/rhi3d/impl/d3d/host/render_device.h"

namespace render {

long SmtD3DRenderDevice::MatrixModeSet(MatrixMode mode) {
  m_matrixMode = mode;
  return SMT_ERR_NONE;
}

MatrixMode SmtD3DRenderDevice::MatrixModeGet() const { return m_matrixMode; }

long SmtD3DRenderDevice::MatrixLoadIdentity() {
  active_matrix().identity();
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixLoad(const Matrix& m) {
  active_matrix() = m;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixPush() {
  if (!modelview_stack_ || !projection_stack_) {
    return SMT_ERR_FAILURE;
  }
  if (m_matrixMode == MM_PROJECTION) {
    if (projection_sp_ < 0 || projection_sp_ >= kMatrixStackMax) {
      return SMT_ERR_NONE;  // soft ignore — do not AV
    }
    projection_stack_[projection_sp_++] = projection_;
  } else {
    if (modelview_sp_ < 0 || modelview_sp_ >= kMatrixStackMax) {
      return SMT_ERR_NONE;
    }
    modelview_stack_[modelview_sp_++] = modelview_;
  }
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixPop() {
  if (!modelview_stack_ || !projection_stack_) {
    return SMT_ERR_FAILURE;
  }
  if (m_matrixMode == MM_PROJECTION) {
    if (projection_sp_ <= 0 || projection_sp_ > kMatrixStackMax) {
      return SMT_ERR_NONE;
    }
    projection_ = projection_stack_[--projection_sp_];
  } else {
    if (modelview_sp_ <= 0 || modelview_sp_ > kMatrixStackMax) {
      return SMT_ERR_NONE;
    }
    modelview_ = modelview_stack_[--modelview_sp_];
  }
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixScale(float x, float y, float z) {
  Matrix s;
  s.identity();
  s.scale(x, y, z);
  active_matrix() *= s;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixTranslation(float x, float y, float z) {
  Matrix t;
  t.identity();
  t.translate(x, y, z);
  active_matrix() *= t;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixRotation(float angle, float x, float y,
                                        float z) {
  // Leftover GL path used degrees; convert for Matrix::rotate_axis (radians).
  Matrix r;
  r.identity();
  const float rad = angle * (3.14159265358979323846f / 180.f);
  r.rotate(rad, x, y, z);
  active_matrix() *= r;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::MatrixMultiply(const Matrix& m) {
  active_matrix() *= m;
  return SMT_ERR_NONE;
}

Matrix SmtD3DRenderDevice::MatrixGet() { return active_matrix(); }

long SmtD3DRenderDevice::SetOrtho(float left, float right, float bottom,
                                  float top, float zNear, float zFar) {
  Matrix& m = active_matrix();
  m.identity();
  const float rl = right - left;
  const float tb = top - bottom;
  const float fn = zFar - zNear;
  if (rl == 0.f || tb == 0.f || fn == 0.f) return SMT_ERR_FAILURE;
  m._11 = 2.f / rl;
  m._22 = 2.f / tb;
  m._33 = -2.f / fn;
  m._41 = -(right + left) / rl;
  m._42 = -(top + bottom) / tb;
  m._43 = -(zFar + zNear) / fn;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetPerspective(float fovy, float aspect, float zNear,
                                        float zFar) {
  // RH perspective matching leftover GL (gluPerspective), Z in [-w, w].
  // Draw path remaps to D3D clip Z [0, w] via a post-multiply fix matrix.
  Matrix& m = active_matrix();
  m.identity();
  if (aspect == 0.f || zNear <= 0.f || zFar <= zNear) {
    return SMT_ERR_FAILURE;
  }
  const float f = 1.f / std::tan(fovy * (3.14159265358979323846f / 360.f));
  m._11 = f / aspect;
  m._22 = f;
  m._33 = (zFar + zNear) / (zNear - zFar);
  m._34 = -1.f;
  m._43 = (2.f * zFar * zNear) / (zNear - zFar);
  m._44 = 0.f;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetViewLookAt(Vector3& vPos, Vector3& vView,
                                       Vector3& vUp) {
  // gluLookAt-compatible RH view (leftover camera / StereoHwnd orbit).
  float fx = vView.x - vPos.x;
  float fy = vView.y - vPos.y;
  float fz = vView.z - vPos.z;
  float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
  if (fl < 1e-6f) {
    return SMT_ERR_FAILURE;
  }
  fx /= fl;
  fy /= fl;
  fz /= fl;
  // s = normalize(cross(f, up))
  float sx = fy * vUp.z - fz * vUp.y;
  float sy = fz * vUp.x - fx * vUp.z;
  float sz = fx * vUp.y - fy * vUp.x;
  float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
  if (sl < 1e-6f) {
    return SMT_ERR_FAILURE;
  }
  sx /= sl;
  sy /= sl;
  sz /= sl;
  // u = cross(s, f)
  const float ux = sy * fz - sz * fy;
  const float uy = sz * fx - sx * fz;
  const float uz = sx * fy - sy * fx;

  Matrix& m = active_matrix();
  m.identity();
  m._11 = sx;
  m._21 = sy;
  m._31 = sz;
  m._12 = ux;
  m._22 = uy;
  m._32 = uz;
  m._13 = -fx;
  m._23 = -fy;
  m._33 = -fz;
  m._41 = -(sx * vPos.x + sy * vPos.y + sz * vPos.z);
  m._42 = -(ux * vPos.x + uy * vPos.y + uz * vPos.z);
  m._43 = -(-fx * vPos.x - fy * vPos.y - fz * vPos.z);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::GetFrustum(SmtFrustum& frustum) {
  // Match leftover GL: clip = modelview * projection (column-major multiply
  // over OpenGL-order float[16]), then extract + normalize 6 planes.
  const Matrix& mod = modelview_;
  const Matrix& proj = projection_;
  float m[16] = {mod._11, mod._12, mod._13, mod._14, mod._21, mod._22,
                 mod._23, mod._24, mod._31, mod._32, mod._33, mod._34,
                 mod._41, mod._42, mod._43, mod._44};
  float p[16] = {proj._11, proj._12, proj._13, proj._14, proj._21, proj._22,
                 proj._23, proj._24, proj._31, proj._32, proj._33, proj._34,
                 proj._41, proj._42, proj._43, proj._44};
  // GL stores column-major; Matrix is row-major D3DX layout. Convert by
  // transposing into the column-major arrays the leftover extract expects.
  float modl[16] = {m[0], m[4], m[8],  m[12], m[1], m[5], m[9],  m[13],
                    m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]};
  float proj_cm[16] = {p[0], p[4], p[8],  p[12], p[1], p[5], p[9],  p[13],
                       p[2], p[6], p[10], p[14], p[3], p[7], p[11], p[15]};

  float clip[16];
  clip[0] = modl[0] * proj_cm[0] + modl[1] * proj_cm[4] + modl[2] * proj_cm[8] +
            modl[3] * proj_cm[12];
  clip[1] = modl[0] * proj_cm[1] + modl[1] * proj_cm[5] + modl[2] * proj_cm[9] +
            modl[3] * proj_cm[13];
  clip[2] = modl[0] * proj_cm[2] + modl[1] * proj_cm[6] +
            modl[2] * proj_cm[10] + modl[3] * proj_cm[14];
  clip[3] = modl[0] * proj_cm[3] + modl[1] * proj_cm[7] +
            modl[2] * proj_cm[11] + modl[3] * proj_cm[15];
  clip[4] = modl[4] * proj_cm[0] + modl[5] * proj_cm[4] + modl[6] * proj_cm[8] +
            modl[7] * proj_cm[12];
  clip[5] = modl[4] * proj_cm[1] + modl[5] * proj_cm[5] + modl[6] * proj_cm[9] +
            modl[7] * proj_cm[13];
  clip[6] = modl[4] * proj_cm[2] + modl[5] * proj_cm[6] +
            modl[6] * proj_cm[10] + modl[7] * proj_cm[14];
  clip[7] = modl[4] * proj_cm[3] + modl[5] * proj_cm[7] +
            modl[6] * proj_cm[11] + modl[7] * proj_cm[15];
  clip[8] = modl[8] * proj_cm[0] + modl[9] * proj_cm[4] +
            modl[10] * proj_cm[8] + modl[11] * proj_cm[12];
  clip[9] = modl[8] * proj_cm[1] + modl[9] * proj_cm[5] +
            modl[10] * proj_cm[9] + modl[11] * proj_cm[13];
  clip[10] = modl[8] * proj_cm[2] + modl[9] * proj_cm[6] +
             modl[10] * proj_cm[10] + modl[11] * proj_cm[14];
  clip[11] = modl[8] * proj_cm[3] + modl[9] * proj_cm[7] +
             modl[10] * proj_cm[11] + modl[11] * proj_cm[15];
  clip[12] = modl[12] * proj_cm[0] + modl[13] * proj_cm[4] +
             modl[14] * proj_cm[8] + modl[15] * proj_cm[12];
  clip[13] = modl[12] * proj_cm[1] + modl[13] * proj_cm[5] +
             modl[14] * proj_cm[9] + modl[15] * proj_cm[13];
  clip[14] = modl[12] * proj_cm[2] + modl[13] * proj_cm[6] +
             modl[14] * proj_cm[10] + modl[15] * proj_cm[14];
  clip[15] = modl[12] * proj_cm[3] + modl[13] * proj_cm[7] +
             modl[14] * proj_cm[11] + modl[15] * proj_cm[15];

  float planes[6][4];
  auto normalize = [](float plane[4]) {
    const float len = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] +
                                plane[2] * plane[2]);
    if (len > 1e-8f) {
      plane[0] /= len;
      plane[1] /= len;
      plane[2] /= len;
      plane[3] /= len;
    }
  };

  planes[FS_RIGHT][P_A] = clip[3] - clip[0];
  planes[FS_RIGHT][P_B] = clip[7] - clip[4];
  planes[FS_RIGHT][P_C] = clip[11] - clip[8];
  planes[FS_RIGHT][P_D] = clip[15] - clip[12];
  normalize(planes[FS_RIGHT]);

  planes[FS_LEFT][P_A] = clip[3] + clip[0];
  planes[FS_LEFT][P_B] = clip[7] + clip[4];
  planes[FS_LEFT][P_C] = clip[11] + clip[8];
  planes[FS_LEFT][P_D] = clip[15] + clip[12];
  normalize(planes[FS_LEFT]);

  planes[FS_BOTTOM][P_A] = clip[3] + clip[1];
  planes[FS_BOTTOM][P_B] = clip[7] + clip[5];
  planes[FS_BOTTOM][P_C] = clip[11] + clip[9];
  planes[FS_BOTTOM][P_D] = clip[15] + clip[13];
  normalize(planes[FS_BOTTOM]);

  planes[FS_TOP][P_A] = clip[3] - clip[1];
  planes[FS_TOP][P_B] = clip[7] - clip[5];
  planes[FS_TOP][P_C] = clip[11] - clip[9];
  planes[FS_TOP][P_D] = clip[15] - clip[13];
  normalize(planes[FS_TOP]);

  planes[FS_BACK][P_A] = clip[3] - clip[2];
  planes[FS_BACK][P_B] = clip[7] - clip[6];
  planes[FS_BACK][P_C] = clip[11] - clip[10];
  planes[FS_BACK][P_D] = clip[15] - clip[14];
  normalize(planes[FS_BACK]);

  planes[FS_FRONT][P_A] = clip[3] + clip[2];
  planes[FS_FRONT][P_B] = clip[7] + clip[6];
  planes[FS_FRONT][P_C] = clip[11] + clip[10];
  planes[FS_FRONT][P_D] = clip[15] + clip[14];
  normalize(planes[FS_FRONT]);

  frustum.SetFrustum(planes);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::Transform2DTo3D(Vector3& /*vOrg*/, Vector3& /*vTar*/,
                                         const lPoint& /*point*/) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::Transform3DTo2D(const Vector3& ver3D, lPoint& point) {
  // Match gluProject: bottom-up window Y; MapLabelBatch flips to top-down.
  const Matrix mvp = modelview_ * projection_;
  const Vector4 ndc =
      mvp.transform_point(Vector4(ver3D.x, ver3D.y, ver3D.z, 1.f));
  const float vw = static_cast<float>(
      m_viewPort.ulWidth > 0 ? m_viewPort.ulWidth : backbuffer_width_);
  const float vh = static_cast<float>(
      m_viewPort.ulHeight > 0 ? m_viewPort.ulHeight : backbuffer_height_);
  if (vw <= 0.f || vh <= 0.f) {
    return SMT_ERR_FAILURE;
  }
  point.x = static_cast<long>(m_viewPort.ulX + (ndc.x + 1.f) * 0.5f * vw);
  point.y = static_cast<long>(m_viewPort.ulY + (ndc.y + 1.f) * 0.5f * vh);
  return SMT_ERR_NONE;
}

}  // namespace render
