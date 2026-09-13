// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_QUAT_H_
#define SMT_RENDER_MATH_QUAT_H_

#include "render/math/matrix.h"

#include <Eigen/Geometry>

namespace render {

// Unit quaternion for orientation. Stored as (x,y,z,w) with Eigen (w,x,y,z).
class Quat {
 public:
  float x, y, z, w;

  Quat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
  Quat(float x_in, float y_in, float z_in, float w_in)
      : x(x_in), y(y_in), z(z_in), w(w_in) {}

  Eigen::Quaternionf eigen() const { return Eigen::Quaternionf(w, x, y, z); }
  void from_eigen(const Eigen::Quaternionf& q) {
    x = q.x();
    y = q.y();
    z = q.z();
    w = q.w();
  }

  void from_euler(float pitch, float yaw, float roll) {
    from_eigen(Eigen::AngleAxisf(yaw, EigenVec3::UnitY()) *
               Eigen::AngleAxisf(pitch, EigenVec3::UnitX()) *
               Eigen::AngleAxisf(roll, EigenVec3::UnitZ()));
  }
  void normalize() {
    Eigen::Quaternionf q = eigen();
    q.normalize();
    from_eigen(q);
  }
  void conjugate(const Quat& q) { from_eigen(q.eigen().conjugate()); }
  void to_euler(float* pitch, float* yaw, float* roll) const {
    const EigenVec3 e = eigen().toRotationMatrix().eulerAngles(1, 0, 2);
    if (yaw) {
      *yaw = e[0];
    }
    if (pitch) {
      *pitch = e[1];
    }
    if (roll) {
      *roll = e[2];
    }
  }
  void to_matrix(Matrix* m) const {
    if (m == nullptr) {
      return;
    }
    m->identity();
    const Eigen::Matrix3f r = eigen().toRotationMatrix();
    m->_11 = r(0, 0);
    m->_12 = r(0, 1);
    m->_13 = r(0, 2);
    m->_21 = r(1, 0);
    m->_22 = r(1, 1);
    m->_23 = r(1, 2);
    m->_31 = r(2, 0);
    m->_32 = r(2, 1);
    m->_33 = r(2, 2);
  }
  float magnitude() const { return eigen().norm(); }

  void operator/=(float f) {
    x /= f;
    y /= f;
    z /= f;
    w /= f;
  }
  Quat operator/(float f) const { return Quat(x / f, y / f, z / f, w / f); }
  void operator*=(float f) {
    x *= f;
    y *= f;
    z *= f;
    w *= f;
  }
  Quat operator*(float f) const { return Quat(x * f, y * f, z * f, w * f); }
  Quat operator*(const Vector4& v) const {
    return Quat(w * v.x + y * v.z - z * v.y, w * v.y + z * v.x - x * v.z,
                w * v.z + x * v.y - y * v.x, -(x * v.x + y * v.y + z * v.z));
  }
  Quat operator*(const Quat& q) const {
    Quat out;
    out.from_eigen(eigen() * q.eigen());
    return out;
  }
  void operator*=(const Quat& q) { from_eigen(eigen() * q.eigen()); }
  void operator+=(const Quat& q) {
    x += q.x;
    y += q.y;
    z += q.z;
    w += q.w;
  }
  Quat operator+(const Quat& q) const {
    return Quat(x + q.x, y + q.y, z + q.z, w + q.w);
  }
  Quat operator~() const { return Quat(-x, -y, -z, w); }

  void rotate(const Quat& q1, const Quat& q2) {
    from_eigen(q1.eigen() * q2.eigen());
  }
  Vector4 rotate_vector(const Vector4& v) const {
    const EigenVec3 r = eigen() * v.xyz();
    return Vector4(r.x(), r.y(), r.z());
  }
};

inline Quat Vector4::operator*(const Quat& q) const {
  return Quat(q.w * x + q.z * y - q.y * z, q.w * y + q.x * z - q.z * x,
              q.w * z + q.y * x - q.x * y, -(q.x * x + q.y * y + q.z * z));
}

inline Quat Vector3::operator*(const Quat& q) const {
  return Quat(q.w * x + q.z * y - q.y * z, q.w * y + q.x * z - q.z * x,
              q.w * z + q.y * x - q.x * y, -(q.x * x + q.y * y + q.z * z));
}

}  // namespace render

#endif  // SMT_RENDER_MATH_QUAT_H_
