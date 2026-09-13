// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_VECTOR_H_
#define SMT_RENDER_MATH_VECTOR_H_

#include "render/math/constants.h"

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <cmath>

namespace render {

class Matrix;
class Quat;
class Vector3;
class Vector4;

using EigenVec2 = Eigen::Matrix<float, 2, 1>;
using EigenVec3 = Eigen::Matrix<float, 3, 1>;
using EigenVec4 = Eigen::Matrix<float, 4, 1>;

// 4D scene vector. Linear algebra goes through an unaligned Eigen map of xyz.
class Vector4 {
 public:
  using coordinate_type = float;
  static constexpr int dimension = 4;

  float x, y, z, w;

  Vector4() : x(0), y(0), z(0), w(1.0f) {}
  Vector4(float x_in, float y_in, float z_in)
      : x(x_in), y(y_in), z(z_in), w(1.0f) {}
  Vector4(float x_in, float y_in, float z_in, float w_in)
      : x(x_in), y(y_in), z(z_in), w(w_in) {}
  Vector4(const Vector3& v);

  using Map = Eigen::Map<EigenVec3, Eigen::Unaligned>;
  using ConstMap = Eigen::Map<const EigenVec3, Eigen::Unaligned>;

  Map xyz() { return Map(&x); }
  ConstMap xyz() const { return ConstMap(&x); }

  void set(float x_in, float y_in, float z_in, float w_in = 1.0f) {
    x = x_in;
    y = y_in;
    z = z_in;
    w = w_in;
  }
  float length() const { return xyz().norm(); }
  float length_squared() const { return xyz().squaredNorm(); }
  void negate() { xyz() = -xyz(); }
  void normalize() {
    const float n = xyz().norm();
    if (n != 0.0f) {
      xyz() /= n;
    }
  }
  float angle_with(const Vector4& v) const {
    return std::acos(dot(v) / (length() * v.length()));
  }
  void difference(const Vector4& v1, const Vector4& v2) {
    xyz() = v2.xyz() - v1.xyz();
    w = 1.0f;
  }
  Vector4 cross(const Vector4& v) const {
    Vector4 out;
    out.xyz() = xyz().cross(v.xyz());
    out.w = 1.0f;
    return out;
  }
  float dot(const Vector4& v) const { return xyz().dot(v.xyz()); }

  void rotate_with(const Matrix& m);
  void inv_rotate_with(const Matrix& m);

  void operator+=(const Vector4& v) { xyz() += v.xyz(); }
  void operator-=(const Vector4& v) { xyz() -= v.xyz(); }
  void operator*=(float f) { xyz() *= f; }
  void operator/=(float f) { xyz() /= f; }
  void operator+=(float f) { xyz() += EigenVec3::Constant(f); }
  void operator-=(float f) { xyz() -= EigenVec3::Constant(f); }
  Vector4 operator*(float f) const { return Vector4(x * f, y * f, z * f); }
  Vector4 operator/(float f) const { return Vector4(x / f, y / f, z / f); }
  Vector4 operator+(float f) const { return Vector4(x + f, y + f, z + f); }
  Vector4 operator-(float f) const { return Vector4(x - f, y - f, z - f); }
  Quat operator*(const Quat& q) const;
  Vector4 operator*(const Matrix& m) const;
  Vector4 operator+(const Vector4& v) const {
    return Vector4(x + v.x, y + v.y, z + v.z);
  }
  Vector4 operator-(const Vector4& v) const {
    return Vector4(x - v.x, y - v.y, z - v.z);
  }
};

// 3D scene vector. Storage matches leftover `.x/.y/.z` field layout.
class Vector3 {
 public:
  using coordinate_type = float;
  static constexpr int dimension = 3;

  float x, y, z;

  Vector3() : x(0), y(0), z(0) {}
  Vector3(float x_in, float y_in, float z_in) : x(x_in), y(y_in), z(z_in) {}
  Vector3(const Vector4& v) : x(v.x), y(v.y), z(v.z) {}

  using Map = Eigen::Map<EigenVec3, Eigen::Unaligned>;
  using ConstMap = Eigen::Map<const EigenVec3, Eigen::Unaligned>;

  Map eigen() { return Map(&x); }
  ConstMap eigen() const { return ConstMap(&x); }

  void set(float x_in, float y_in, float z_in) {
    x = x_in;
    y = y_in;
    z = z_in;
  }
  float length() const { return eigen().norm(); }
  float length_squared() const { return eigen().squaredNorm(); }
  void negate() { eigen() = -eigen(); }
  void normalize() {
    const float n = eigen().norm();
    if (n != 0.0f) {
      eigen() /= n;
    }
  }
  float angle_with(const Vector3& v) const {
    return std::acos(dot(v) / (length() * v.length()));
  }
  void difference(const Vector3& v1, const Vector3& v2) {
    eigen() = v2.eigen() - v1.eigen();
  }
  void rotate(const Vector3& axis, float theta) {
    const Eigen::AngleAxisf aa(theta, axis.eigen().normalized());
    eigen() = aa * eigen();
  }
  Vector3 cross(const Vector3& v) const {
    Vector3 out;
    out.eigen() = eigen().cross(v.eigen());
    return out;
  }
  float dot(const Vector3& v) const { return eigen().dot(v.eigen()); }

  void operator+=(const Vector3& v) { eigen() += v.eigen(); }
  void operator-=(const Vector3& v) { eigen() -= v.eigen(); }
  void operator*=(float f) { eigen() *= f; }
  void operator/=(float f) { eigen() /= f; }
  void operator+=(float f) { eigen() += EigenVec3::Constant(f); }
  void operator-=(float f) { eigen() -= EigenVec3::Constant(f); }
  Vector3 operator*(float f) const { return Vector3(x * f, y * f, z * f); }
  Vector3 operator/(float f) const { return Vector3(x / f, y / f, z / f); }
  Vector3 operator+(float f) const { return Vector3(x + f, y + f, z + f); }
  Vector3 operator-(float f) const { return Vector3(x - f, y - f, z - f); }
  Quat operator*(const Quat& q) const;
  Vector3 operator+(const Vector3& v) const {
    return Vector3(x + v.x, y + v.y, z + v.z);
  }
  Vector3 operator-(const Vector3& v) const {
    return Vector3(x - v.x, y - v.y, z - v.z);
  }
};

inline Vector4::Vector4(const Vector3& v)
    : x(v.x), y(v.y), z(v.z), w(1.0f) {}

// 2D scene vector.
class Vector2 {
 public:
  using coordinate_type = float;
  static constexpr int dimension = 2;

  float x, y;

  Vector2() : x(0), y(0) {}
  Vector2(float x_in, float y_in) : x(x_in), y(y_in) {}

  using Map = Eigen::Map<EigenVec2, Eigen::Unaligned>;
  using ConstMap = Eigen::Map<const EigenVec2, Eigen::Unaligned>;

  Map eigen() { return Map(&x); }
  ConstMap eigen() const { return ConstMap(&x); }

  void set(float x_in, float y_in) {
    x = x_in;
    y = y_in;
  }
  float length() const { return eigen().norm(); }
  float length_squared() const { return eigen().squaredNorm(); }
  void negate() { eigen() = -eigen(); }
  void normalize() {
    const float n = eigen().norm();
    if (n != 0.0f) {
      eigen() /= n;
    }
  }
  float angle_with(const Vector2& v) const {
    return std::acos(dot(v) / (length() * v.length()));
  }
  void difference(const Vector2& v1, const Vector2& v2) {
    eigen() = v2.eigen() - v1.eigen();
  }
  void rotate(const Vector2& axis, float theta) {
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const float ax = axis.x;
    const float ay = axis.y;
    const float nx = (c + (1 - c) * ax * ax) * x + ((1 - c) * ax * ay) * y;
    const float ny = ((1 - c) * ax * ay) * x + (c + (1 - c) * ay * ay) * y;
    x = nx;
    y = ny;
  }
  double cross(const Vector2& v) const {
    return static_cast<double>(x * v.y - y * v.x);
  }
  float dot(const Vector2& v) const { return eigen().dot(v.eigen()); }

  void operator+=(const Vector2& v) { eigen() += v.eigen(); }
  void operator-=(const Vector2& v) { eigen() -= v.eigen(); }
  void operator*=(float f) { eigen() *= f; }
  void operator/=(float f) { eigen() /= f; }
  void operator+=(float f) { eigen() += EigenVec2::Constant(f); }
  void operator-=(float f) { eigen() -= EigenVec2::Constant(f); }
  Vector2 operator*(float f) const { return Vector2(x * f, y * f); }
  Vector2 operator/(float f) const { return Vector2(x / f, y / f); }
  Vector2 operator+(float f) const { return Vector2(x + f, y + f); }
  Vector2 operator-(float f) const { return Vector2(x - f, y - f); }
  Vector2 operator+(const Vector2& v) const { return Vector2(x + v.x, y + v.y); }
  Vector2 operator-(const Vector2& v) const { return Vector2(x - v.x, y - v.y); }
};

inline float dot(const Vector2& a, const Vector2& b) { return a.dot(b); }
inline float dot(const Vector3& a, const Vector3& b) { return a.dot(b); }
inline float dot(const Vector4& a, const Vector4& b) { return a.dot(b); }

inline double cross(const Vector2& a, const Vector2& b) { return a.cross(b); }
inline Vector3 cross(const Vector3& a, const Vector3& b) { return a.cross(b); }
inline Vector4 cross(const Vector4& a, const Vector4& b) { return a.cross(b); }

inline Vector4 triangle_normal(const Vector4& v1, const Vector4& v2,
                               const Vector4& v3) {
  Vector4 normal = (v1 - v2).cross(v1 - v3);
  normal.normalize();
  return normal;
}

}  // namespace render

#endif  // SMT_RENDER_MATH_VECTOR_H_
