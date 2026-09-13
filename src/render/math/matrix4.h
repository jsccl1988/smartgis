// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_MATRIX_H_
#define SMT_RENDER_MATH_MATRIX_H_

#include "render/math/vector.h"

#include <cmath>
#include <cstring>

namespace render {

using EigenMat4 = Eigen::Matrix<float, 4, 4, Eigen::RowMajor>;

// 4x4 leftover matrix. Memory layout matches D3DXMATRIX / glLoadMatrixf.
class Matrix {
 public:
  float _11, _12, _13, _14;
  float _21, _22, _23, _24;
  float _31, _32, _33, _34;
  float _41, _42, _43, _44;

  Matrix() = default;

  using Map = Eigen::Map<EigenMat4, Eigen::Unaligned>;
  using ConstMap = Eigen::Map<const EigenMat4, Eigen::Unaligned>;

  Map eigen() { return Map(&_11); }
  ConstMap eigen() const { return ConstMap(&_11); }

  void identity() { eigen().setIdentity(); }
  void rotate_x(float a);
  void rotate_y(float a);
  void rotate_z(float a);
  void rotate_euler(const Vector4& vc);
  void rotate_euler(float x, float y, float z) {
    rotate_euler(Vector4(x, y, z));
  }
  void rotate_axis(const Vector4& axis, float a);
  void apply_inverse_rotation(Vector4* pvc);

  void translate(float dx, float dy, float dz) {
    _41 = dx;
    _42 = dy;
    _43 = dz;
  }
  void rotate(float angle, float x, float y, float z) {
    rotate_axis(Vector4(x, y, z), angle);
  }
  void scale(float sx, float sy, float sz) {
    _11 = sx;
    _22 = sy;
    _33 = sz;
  }

  void set_translation(Vector4 vc, bool erase_content = false) {
    if (erase_content) {
      identity();
    }
    _41 = vc.x;
    _42 = vc.y;
    _43 = vc.z;
  }
  Vector4 translation() const { return Vector4(_41, _42, _43); }

  void set_perspective(float fovy, float aspect, float z_near, float z_far);
  void billboard(Vector4 pos, Vector4 dir,
                 Vector4 world_up = Vector4(0, 1, 0));
  void look_at(Vector4 pos, Vector4 look_at,
               Vector4 world_up = Vector4(0, 1, 0));

  void transpose_of(const Matrix& m) { eigen() = m.eigen().transpose(); }
  void inverse_of(const Matrix& m) { eigen() = m.eigen().inverse(); }

  // Point transform with perspective divide (homogeneous w).
  Vector4 transform_point(const Vector4& vc) const;
  // Direction / normal: 3x3 part only, no translation or divide.
  Vector4 transform_vector(const Vector4& vc) const;

  Matrix operator*(const Matrix& m) const {
    Matrix out;
    out.eigen() = eigen() * m.eigen();
    return out;
  }
  void operator*=(const Matrix& m) { eigen() = eigen() * m.eigen(); }
  Vector4 operator*(const Vector4& vc) const { return transform_point(vc); }
};

inline void Vector4::rotate_with(const Matrix& m) {
  const float nx = x * m._11 + y * m._21 + z * m._31;
  const float ny = x * m._12 + y * m._22 + z * m._32;
  const float nz = x * m._13 + y * m._23 + z * m._33;
  x = nx;
  y = ny;
  z = nz;
}

inline void Vector4::inv_rotate_with(const Matrix& m) {
  const float nx = x * m._11 + y * m._12 + z * m._13;
  const float ny = x * m._21 + y * m._22 + z * m._23;
  const float nz = x * m._31 + y * m._32 + z * m._33;
  x = nx;
  y = ny;
  z = nz;
}

inline Vector4 Vector4::operator*(const Matrix& m) const {
  return m.transform_point(*this);
}

inline void Matrix::rotate_x(float a) {
  const float c = std::cosf(a);
  const float s = std::sinf(a);
  identity();
  _22 = c;
  _23 = s;
  _32 = -s;
  _33 = c;
}

inline void Matrix::rotate_y(float a) {
  const float c = std::cosf(a);
  const float s = std::sinf(a);
  identity();
  _11 = c;
  _13 = -s;
  _31 = s;
  _33 = c;
}

inline void Matrix::rotate_z(float a) {
  const float c = std::cosf(a);
  const float s = std::sinf(a);
  identity();
  _11 = c;
  _12 = s;
  _21 = -s;
  _22 = c;
}

inline void Matrix::rotate_euler(const Vector4& vc) {
  identity();
  const float sy = std::sinf(vc.z);
  const float cy = std::cosf(vc.z);
  const float sp = std::sinf(vc.y);
  const float cp = std::cosf(vc.y);
  const float sr = std::sinf(vc.x);
  const float cr = std::cosf(vc.x);
  _11 = cp * cy;
  _12 = cp * sy;
  _13 = -sp;
  _21 = sr * sp * cy + cr * -sy;
  _22 = sr * sp * sy + cr * cy;
  _23 = sr * cp;
  _31 = cr * sp * cy + -sr * -sy;
  _32 = cr * sp * sy + -sr * cy;
  _33 = cr * cp;
}

inline void Matrix::rotate_axis(const Vector4& vc_axis, float a) {
  Vector4 axis = vc_axis;
  if (axis.length_squared() != 1.0f) {
    axis.normalize();
  }
  const Eigen::AngleAxisf aa(a, axis.xyz());
  const Eigen::Matrix3f r = aa.toRotationMatrix();
  identity();
  _11 = r(0, 0);
  _12 = r(0, 1);
  _13 = r(0, 2);
  _21 = r(1, 0);
  _22 = r(1, 1);
  _23 = r(1, 2);
  _31 = r(2, 0);
  _32 = r(2, 1);
  _33 = r(2, 2);
}

inline void Matrix::apply_inverse_rotation(Vector4* pvc) {
  if (pvc == nullptr) {
    return;
  }
  pvc->inv_rotate_with(*this);
  pvc->w = 1.0f;
}

inline void Matrix::set_perspective(float fovy, float aspect, float z_near,
                                    float z_far) {
  if (std::fabs(z_far - z_near) < 0.01f) {
    return;
  }
  const float sin_fov2 =
      std::sinf(static_cast<float>(0.5f * deg_to_rad(fovy)));
  if (std::fabs(sin_fov2) < 0.01f) {
    return;
  }
  const float cos_fov2 =
      std::cosf(static_cast<float>(0.5f * deg_to_rad(fovy)));
  const float w = aspect * (cos_fov2 / sin_fov2);
  const float h = 1.0f * (cos_fov2 / sin_fov2);
  const float q = z_far / (z_far - z_near);
  _11 = w;
  _22 = h;
  _33 = q;
  _43 = -q * z_near;
  _34 = 1.0f;
}

inline void Matrix::look_at(Vector4 pos, Vector4 look_at_pt, Vector4 world_up) {
  Vector4 dir = look_at_pt - pos;
  dir.normalize();
  const float angle = dot(world_up, dir);
  Vector4 up = world_up - (dir * angle);
  up.normalize();
  const Vector4 right = up.cross(dir);
  _11 = right.x;
  _21 = up.x;
  _31 = dir.x;
  _12 = right.y;
  _22 = up.y;
  _32 = dir.y;
  _13 = right.z;
  _23 = up.z;
  _33 = dir.z;
  _41 = pos.x;
  _42 = pos.y;
  _43 = pos.z;
  _14 = 0.0f;
  _24 = 0.0f;
  _34 = 0.0f;
  _44 = 1.0f;
}

inline void Matrix::billboard(Vector4 pos, Vector4 dir, Vector4 world_up) {
  const float angle = dot(world_up, dir);
  Vector4 up = world_up - (dir * angle);
  up.normalize();
  const Vector4 right = up.cross(dir);
  _11 = right.x;
  _21 = up.x;
  _31 = dir.x;
  _12 = right.y;
  _22 = up.y;
  _32 = dir.y;
  _13 = right.z;
  _23 = up.z;
  _33 = dir.z;
  _41 = pos.x;
  _42 = pos.y;
  _43 = pos.z;
  _14 = 0.0f;
  _24 = 0.0f;
  _34 = 0.0f;
  _44 = 1.0f;
}

inline Vector4 Matrix::transform_point(const Vector4& vc) const {
  Vector4 out;
  out.x = vc.x * _11 + vc.y * _21 + vc.z * _31 + _41;
  out.y = vc.x * _12 + vc.y * _22 + vc.z * _32 + _42;
  out.z = vc.x * _13 + vc.y * _23 + vc.z * _33 + _43;
  out.w = vc.x * _14 + vc.y * _24 + vc.z * _34 + _44;
  out.x /= out.w;
  out.y /= out.w;
  out.z /= out.w;
  out.w = 1.0f;
  return out;
}

inline Vector4 Matrix::transform_vector(const Vector4& vc) const {
  Vector4 out;
  out.x = vc.x * _11 + vc.y * _21 + vc.z * _31;
  out.y = vc.x * _12 + vc.y * _22 + vc.z * _32;
  out.z = vc.x * _13 + vc.y * _23 + vc.z * _33;
  out.w = 0.0f;
  return out;
}

}  // namespace render

#endif  // SMT_RENDER_MATH_MATRIX_H_
