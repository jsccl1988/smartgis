// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATHLIB_3D_H
#define SMT_RENDER_MATHLIB_3D_H

#include "base/core/core.h"

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <cmath>
#include <cstring>

// Leftover-compatible 3D math. Storage matches Smt_3DMath field names
// (.x/.y/.z, D3D-style _11.._44). Linear algebra is Eigen.

namespace render {

const double PI = 3.14159265;
const double PI_T = 1.5707963;
const double T_PI = 6.2831853;
const float G = -32.174f;
const float EPSILON = 0.00001f;

#define DEG2RAD(a) (PI / 180 * (a))
#define RAD2DEG(a) (180 / PI * (a))

#ifndef NULL
#define NULL 0
#endif

#define FRONT 0
#define BACK 1
#define PLANAR 2
#define CLIPPED 3
#define CULLED 4
#define VISIBLE 5

inline float _fabs(float f) {
  return f < 0.0f ? -f : f;
}

class Matrix;
class Obb;
class Aabb;
class Plane;
class Quat;
class Vector3;
class Vector4;

using EigenVec2 = Eigen::Matrix<float, 2, 1>;
using EigenVec3 = Eigen::Matrix<float, 3, 1>;
using EigenVec4 = Eigen::Matrix<float, 4, 1>;
using EigenMat4 = Eigen::Matrix<float, 4, 4, Eigen::RowMajor>;

// 4D leftover vector. Math goes through an unaligned Eigen map.
class Vector4 {
 public:
  using coordinate_type = float;
  static constexpr int dimension = 4;

  float x, y, z, w;

  Vector4() : x(0), y(0), z(0), w(1.0f) {}
  Vector4(float _x, float _y, float _z) : x(_x), y(_y), z(_z), w(1.0f) {}
  Vector4(float _x, float _y, float _z, float _w)
      : x(_x), y(_y), z(_z), w(_w) {}
  Vector4(const Vector3& vec3);

  using Map = Eigen::Map<EigenVec3, Eigen::Unaligned>;
  using ConstMap = Eigen::Map<const EigenVec3, Eigen::Unaligned>;

  Map xyz() { return Map(&x); }
  ConstMap xyz() const { return ConstMap(&x); }

  void Set(float _x, float _y, float _z, float _w = 1.0f) {
    x = _x;
    y = _y;
    z = _z;
    w = _w;
  }
  float GetLength() { return xyz().norm(); }
  float GetSqrLength() const { return xyz().squaredNorm(); }
  void Negate() { xyz() = -xyz(); }
  void Normalize() {
    const float n = xyz().norm();
    if (n != 0.0f) {
      xyz() /= n;
    }
  }
  float AngleWith(Vector4& v) {
    return std::acos(((*this) * v) / (GetLength() * v.GetLength()));
  }
  void Difference(const Vector4& v1, const Vector4& v2) {
    xyz() = v2.xyz() - v1.xyz();
    w = 1.0f;
  }
  Vector4 CrossProduct(const Vector4& v1) const {
    Vector4 out;
    out.xyz() = xyz().cross(v1.xyz());
    out.w = 1.0f;
    return out;
  }
  void RotateWith(const Matrix& m);
  void InvRotateWith(const Matrix& m);

  void operator+=(const Vector4& v) { xyz() += v.xyz(); }
  void operator-=(const Vector4& v) { xyz() -= v.xyz(); }
  void operator*=(float f) { xyz() *= f; }
  void operator/=(float f) { xyz() /= f; }
  void operator+=(float f) { xyz() += EigenVec3::Constant(f); }
  void operator-=(float f) { xyz() -= EigenVec3::Constant(f); }
  float operator*(const Vector4& v) const { return xyz().dot(v.xyz()); }
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

// 3D leftover vector. `using Vector3 = Eigen::Vector3f` would break .x
// fields and leftover `v1 * v2` as dot product.
class Vector3 {
 public:
  using coordinate_type = float;
  static constexpr int dimension = 3;

  float x, y, z;

  Vector3() : x(0), y(0), z(0) {}
  Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
  Vector3(const Vector4& vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}

  using Map = Eigen::Map<EigenVec3, Eigen::Unaligned>;
  using ConstMap = Eigen::Map<const EigenVec3, Eigen::Unaligned>;

  Map eigen() { return Map(&x); }
  ConstMap eigen() const { return ConstMap(&x); }

  void Set(float _x, float _y, float _z) {
    x = _x;
    y = _y;
    z = _z;
  }
  float GetLength() { return eigen().norm(); }
  float GetSqrLength() const { return eigen().squaredNorm(); }
  void Negate() { eigen() = -eigen(); }
  void Normalize() {
    const float n = eigen().norm();
    if (n != 0.0f) {
      eigen() /= n;
    }
  }
  float AngleWith(Vector3& v) {
    return std::acos(((*this) * v) / (GetLength() * v.GetLength()));
  }
  void Difference(const Vector3& v1, const Vector3& v2) {
    eigen() = v2.eigen() - v1.eigen();
  }
  void Rotate(const Vector3& vAxis, float theta) {
    const Eigen::AngleAxisf aa(theta, vAxis.eigen().normalized());
    eigen() = aa * eigen();
  }
  Vector3 CrossProduct(const Vector3& v1) const {
    Vector3 out;
    out.eigen() = eigen().cross(v1.eigen());
    return out;
  }

  void operator+=(const Vector3& v) { eigen() += v.eigen(); }
  void operator-=(const Vector3& v) { eigen() -= v.eigen(); }
  void operator*=(float f) { eigen() *= f; }
  void operator/=(float f) { eigen() /= f; }
  void operator+=(float f) { eigen() += EigenVec3::Constant(f); }
  void operator-=(float f) { eigen() -= EigenVec3::Constant(f); }
  float operator*(const Vector3& v) const { return eigen().dot(v.eigen()); }
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

inline Vector4::Vector4(const Vector3& vec3)
    : x(vec3.x), y(vec3.y), z(vec3.z), w(1.0f) {}

// 2D leftover vector.
class Vector2 {
 public:
  using coordinate_type = float;
  static constexpr int dimension = 2;

  float x, y;

  Vector2() : x(0), y(0) {}
  Vector2(float _x, float _y) : x(_x), y(_y) {}

  using Map = Eigen::Map<EigenVec2, Eigen::Unaligned>;
  using ConstMap = Eigen::Map<const EigenVec2, Eigen::Unaligned>;

  Map eigen() { return Map(&x); }
  ConstMap eigen() const { return ConstMap(&x); }

  void Set(float _x, float _y) {
    x = _x;
    y = _y;
  }
  float GetLength() { return eigen().norm(); }
  float GetSqrLength() const { return eigen().squaredNorm(); }
  void Negate() { eigen() = -eigen(); }
  void Normalize() {
    const float n = eigen().norm();
    if (n != 0.0f) {
      eigen() /= n;
    }
  }
  float AngleWith(Vector2& v) {
    return std::acos(((*this) * v) / (GetLength() * v.GetLength()));
  }
  void Difference(const Vector2& v1, const Vector2& v2) {
    eigen() = v2.eigen() - v1.eigen();
  }
  void Rotate(const Vector2& vAxis, float theta) {
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const float ax = vAxis.x;
    const float ay = vAxis.y;
    const float nx = (c + (1 - c) * ax * ax) * x + ((1 - c) * ax * ay) * y;
    const float ny = ((1 - c) * ax * ay) * x + (c + (1 - c) * ay * ay) * y;
    x = nx;
    y = ny;
  }
  double CrossProduct(const Vector2& v1) const {
    return static_cast<double>(x * v1.y - y * v1.x);
  }

  void operator+=(const Vector2& v) { eigen() += v.eigen(); }
  void operator-=(const Vector2& v) { eigen() -= v.eigen(); }
  void operator*=(float f) { eigen() *= f; }
  void operator/=(float f) { eigen() /= f; }
  void operator+=(float f) { eigen() += EigenVec2::Constant(f); }
  void operator-=(float f) { eigen() -= EigenVec2::Constant(f); }
  float operator*(const Vector2& v) const { return eigen().dot(v.eigen()); }
  Vector2 operator*(float f) const { return Vector2(x * f, y * f); }
  Vector2 operator/(float f) const { return Vector2(x / f, y / f); }
  Vector2 operator+(float f) const { return Vector2(x + f, y + f); }
  Vector2 operator-(float f) const { return Vector2(x - f, y - f); }
  Vector2 operator+(const Vector2& v) const { return Vector2(x + v.x, y + v.y); }
  Vector2 operator-(const Vector2& v) const { return Vector2(x - v.x, y - v.y); }
};

// 4x4 leftover matrix. Memory layout matches D3DXMATRIX / glLoadMatrixf.
class Matrix {
 public:
  float _11, _12, _13, _14;
  float _21, _22, _23, _24;
  float _31, _32, _33, _34;
  float _41, _42, _43, _44;

  Matrix() {}

  using Map = Eigen::Map<EigenMat4, Eigen::Unaligned>;
  using ConstMap = Eigen::Map<const EigenMat4, Eigen::Unaligned>;

  Map eigen() { return Map(&_11); }
  ConstMap eigen() const { return ConstMap(&_11); }

  void Identity() { eigen().setIdentity(); }
  void RotaX(float a);
  void RotaY(float a);
  void RotaZ(float a);
  void Rota(const Vector4& vc);
  void Rota(float x, float y, float z) { Rota(Vector4(x, y, z)); }
  void RotaArbi(const Vector4& vcAxis, float a);
  void ApplyInverseRota(Vector4* pvc);

  void Translate(float dx, float dy, float dz) {
    _41 = dx;
    _42 = dy;
    _43 = dz;
  }
  void Rotate(float angle, float x, float y, float z) {
    RotaArbi(Vector4(x, y, z), angle);
  }
  void Scale(float sx, float sy, float sz) {
    _11 = sx;
    _22 = sy;
    _33 = sz;
  }

  void SetTranslation(Vector4 vc, bool erase_content = false) {
    if (erase_content) {
      Identity();
    }
    _41 = vc.x;
    _42 = vc.y;
    _43 = vc.z;
  }
  Vector4 GetTranslation() { return Vector4(_41, _42, _43); }

  void SetPerspective(float fovy, float aspect, float z_near, float z_far);
  void Billboard(Vector4 vcPos, Vector4 vcDir,
                 Vector4 vcWorldUp = Vector4(0, 1, 0));
  void LookAt(Vector4 vcPos, Vector4 vcLookAt,
              Vector4 vcWorldUp = Vector4(0, 1, 0));

  void TransposeOf(const Matrix& m) { eigen() = m.eigen().transpose(); }
  void InverseOf(const Matrix& m) { eigen() = m.eigen().inverse(); }

  Matrix operator*(const Matrix& m) const {
    Matrix out;
    out.eigen() = eigen() * m.eigen();
    return out;
  }
  void operator*=(const Matrix& m) { eigen() = eigen() * m.eigen(); }
  Vector4 operator*(const Vector4& vc) const;
};

class Ray {
 public:
  Vector4 m_vcOrig;
  Vector4 m_vcDir;

  Ray() {}

  void Set(Vector4 vcOrig, Vector4 vcDir) {
    m_vcOrig = vcOrig;
    m_vcDir = vcDir;
  }
  void DeTransform(const Matrix& m);

  bool Intersects(const Vector4& vc0, const Vector4& vc1, const Vector4& vc2,
                  bool bCull, float* t);
  bool Intersects(const Vector4& vc0, const Vector4& vc1, const Vector4& vc2,
                  bool bCull, float fL, float* t);
  bool Intersects(const Plane& plane, bool bCull, float* t, Vector4* vcHit);
  bool Intersects(const Plane& plane, bool bCull, float fL, float* t,
                  Vector4* vcHit);
  bool Intersects(const Aabb& aabb, float* t);
  bool Intersects(const Aabb& aabb, float fL, float* t);
  bool Intersects(const Obb& obb, float* t);
  bool Intersects(const Obb& obb, float fL, float* t);
};

class Plane {
 public:
  Vector4 m_vcN;
  Vector4 m_vcPoint;
  float m_fD;

  Plane() {}

  void Set(const Vector4& vcN, const Vector4& vcP) {
    m_fD = -(vcN * vcP);
    m_vcN = vcN;
    m_vcPoint = vcP;
  }
  void Set(const Vector4& vcN, const Vector4& vcP, float fD) {
    m_vcN = vcN;
    m_fD = fD;
    m_vcPoint = vcP;
  }
  void Set(const Vector4& v0, const Vector4& v1, const Vector4& v2) {
    const Vector4 edge1 = v1 - v0;
    const Vector4 edge2 = v2 - v0;
    m_vcN = edge1.CrossProduct(edge2);
    m_vcN.Normalize();
    m_fD = -(m_vcN * v0);
    m_vcPoint = v0;
  }
  float Distance(const Vector4& vcPoint);
  int Classify(const Vector4& vcPoint);

  bool Clip(const Ray*, float, Ray*, Ray*);
  bool Intersects(const Vector4& vc0, const Vector4& vc1, const Vector4& vc2);
  bool Intersects(const Plane& plane, Ray* pIntersection);
  bool Intersects(const Aabb& aabb);
  bool Intersects(const Obb& obb);
};

class Obb {
 public:
  float fA0, fA1, fA2;
  Vector4 vcA0, vcA1, vcA2;
  Vector4 vcCenter;

  Obb() {}

  void DeTransform(const Obb& obb, const Matrix& m);
  bool Intersects(const Ray& ray, float* t);
  bool Intersects(const Ray& ray, float fL, float* t);
  bool Intersects(const Obb& obb);
  bool Intersects(const Vector4& v0, const Vector4& v1, const Vector4& v2);
  int Cull(const Plane* pPlanes, int nNumPlanes);

 private:
  void ObbProj(const Obb& obb, const Vector4& vcV, float* pfMin, float* pfMax);
  void TriProj(const Vector4& v0, const Vector4& v1, const Vector4& v2,
               const Vector4& vcV, float* pfMin, float* pfMax);
};

class Aabb {
 public:
  Vector4 vcMin;
  Vector4 vcMax;
  Vector4 vcCenter;

  Aabb() {
    vcMin.Set(SMT_C_INVALID_DBF_VALUE, SMT_C_INVALID_DBF_VALUE,
              SMT_C_INVALID_DBF_VALUE);
    vcMax = vcCenter = vcMin;
  }
  Aabb(Vector4 vcMin, Vector4 vcMax);

  bool is_init() const;
  void merge(const Aabb& aabb);
  void merge(double dfX, double dfY, double dfZ);
  void merge(const Vector4& vVer);
  void Intersect(Aabb const& sOther);
  bool Intersects(Aabb const& other) const;
  bool Contains(Aabb const& other) const;
  bool Contains(Vector3 const& other) const;

  void Construct(const Obb* pObb);
  int Cull(const Plane* pPlanes, int nNumPlanes);
  void GetPlanes(Plane* pPlanes);
  bool Contains(const Ray& ray, float fL);
  bool Intersects(const Ray& ray, float* t);
  bool Intersects(const Ray& ray, float fL, float* t);
  bool Intersects(const Aabb& aabb);
  bool Intersects(const Vector4& vc0);
};

class Quat {
 public:
  float x, y, z, w;

  Quat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
  Quat(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

  Eigen::Quaternionf eigen() const { return Eigen::Quaternionf(w, x, y, z); }
  void from_eigen(const Eigen::Quaternionf& q) {
    x = q.x();
    y = q.y();
    z = q.z();
    w = q.w();
  }

  void MakeFromEuler(float fPitch, float fYaw, float fRoll) {
    from_eigen(Eigen::AngleAxisf(fYaw, EigenVec3::UnitY()) *
               Eigen::AngleAxisf(fPitch, EigenVec3::UnitX()) *
               Eigen::AngleAxisf(fRoll, EigenVec3::UnitZ()));
  }
  void Normalize() {
    Eigen::Quaternionf q = eigen();
    q.normalize();
    from_eigen(q);
  }
  void Conjugate(Quat q) { from_eigen(q.eigen().conjugate()); }
  void GetEulers(float* fPitch, float* fYaw, float* fRoll) {
    const EigenVec3 e = eigen().toRotationMatrix().eulerAngles(1, 0, 2);
    if (fYaw) {
      *fYaw = e[0];
    }
    if (fPitch) {
      *fPitch = e[1];
    }
    if (fRoll) {
      *fRoll = e[2];
    }
  }
  void GetMatrix(Matrix* m) {
    if (m == nullptr) {
      return;
    }
    m->Identity();
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
  float GetMagnitude() { return eigen().norm(); }

  void operator/=(float f) {
    x /= f;
    y /= f;
    z /= f;
    w /= f;
  }
  Quat operator/(float f) { return Quat(x / f, y / f, z / f, w / f); }
  void operator*=(float f) {
    x *= f;
    y *= f;
    z *= f;
    w *= f;
  }
  Quat operator*(float f) { return Quat(x * f, y * f, z * f, w * f); }
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

  void Rotate(const Quat& q1, const Quat& q2) {
    from_eigen(q1.eigen() * q2.eigen());
  }
  Vector4 Rotate(const Vector4& v) {
    const EigenVec3 r = eigen() * v.xyz();
    return Vector4(r.x(), r.y(), r.z());
  }
};

inline Vector4 triangle_normal(const Vector4& v1, const Vector4& v2,
                               const Vector4& v3) {
  Vector4 normal = (v1 - v2).CrossProduct(v1 - v3);
  normal.Normalize();
  return normal;
}

inline Quat Vector4::operator*(const Quat& q) const {
  return Quat(q.w * x + q.z * y - q.y * z, q.w * y + q.x * z - q.z * x,
              q.w * z + q.y * x - q.x * y, -(q.x * x + q.y * y + q.z * z));
}

inline Quat Vector3::operator*(const Quat& q) const {
  return Quat(q.w * x + q.z * y - q.y * z, q.w * y + q.x * z - q.z * x,
              q.w * z + q.y * x - q.x * y, -(q.x * x + q.y * y + q.z * z));
}

inline void Vector4::RotateWith(const Matrix& m) {
  const float nx = x * m._11 + y * m._21 + z * m._31;
  const float ny = x * m._12 + y * m._22 + z * m._32;
  const float nz = x * m._13 + y * m._23 + z * m._33;
  x = nx;
  y = ny;
  z = nz;
}

inline void Vector4::InvRotateWith(const Matrix& m) {
  const float nx = x * m._11 + y * m._12 + z * m._13;
  const float ny = x * m._21 + y * m._22 + z * m._23;
  const float nz = x * m._31 + y * m._32 + z * m._33;
  x = nx;
  y = ny;
  z = nz;
}

inline Vector4 Vector4::operator*(const Matrix& m) const {
  return m * (*this);
}

inline void Matrix::RotaX(float a) {
  const float c = std::cosf(a);
  const float s = std::sinf(a);
  Identity();
  _22 = c;
  _23 = s;
  _32 = -s;
  _33 = c;
}

inline void Matrix::RotaY(float a) {
  const float c = std::cosf(a);
  const float s = std::sinf(a);
  Identity();
  _11 = c;
  _13 = -s;
  _31 = s;
  _33 = c;
}

inline void Matrix::RotaZ(float a) {
  const float c = std::cosf(a);
  const float s = std::sinf(a);
  Identity();
  _11 = c;
  _12 = s;
  _21 = -s;
  _22 = c;
}

inline void Matrix::Rota(const Vector4& vc) {
  Identity();
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

inline void Matrix::RotaArbi(const Vector4& vcAxis, float a) {
  Vector4 axis = vcAxis;
  if (axis.GetSqrLength() != 1.0f) {
    axis.Normalize();
  }
  const Eigen::AngleAxisf aa(a, axis.xyz());
  const Eigen::Matrix3f r = aa.toRotationMatrix();
  Identity();
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

inline void Matrix::ApplyInverseRota(Vector4* pvc) {
  if (pvc == nullptr) {
    return;
  }
  pvc->InvRotateWith(*this);
  pvc->w = 1.0f;
}

inline void Matrix::SetPerspective(float fovy, float aspect, float z_near,
                                   float z_far) {
  if (std::fabs(z_far - z_near) < 0.01f) {
    return;
  }
  const float sin_fov2 = std::sinf(static_cast<float>(0.5f * DEG2RAD(fovy)));
  if (std::fabs(sin_fov2) < 0.01f) {
    return;
  }
  const float cos_fov2 = std::cosf(static_cast<float>(0.5f * DEG2RAD(fovy)));
  const float w = aspect * (cos_fov2 / sin_fov2);
  const float h = 1.0f * (cos_fov2 / sin_fov2);
  const float q = z_far / (z_far - z_near);
  _11 = w;
  _22 = h;
  _33 = q;
  _43 = -q * z_near;
  _34 = 1.0f;
}

inline void Matrix::LookAt(Vector4 vcPos, Vector4 vcLookAt, Vector4 vcWorldUp) {
  Vector4 vcDir = vcLookAt - vcPos;
  vcDir.Normalize();
  const float angle = vcWorldUp * vcDir;
  Vector4 vcUp = vcWorldUp - (vcDir * angle);
  vcUp.Normalize();
  const Vector4 vcRight = vcUp.CrossProduct(vcDir);
  _11 = vcRight.x;
  _21 = vcUp.x;
  _31 = vcDir.x;
  _12 = vcRight.y;
  _22 = vcUp.y;
  _32 = vcDir.y;
  _13 = vcRight.z;
  _23 = vcUp.z;
  _33 = vcDir.z;
  _41 = vcPos.x;
  _42 = vcPos.y;
  _43 = vcPos.z;
  _14 = 0.0f;
  _24 = 0.0f;
  _34 = 0.0f;
  _44 = 1.0f;
}

inline void Matrix::Billboard(Vector4 vcPos, Vector4 vcDir, Vector4 vcWorldUp) {
  const float angle = vcWorldUp * vcDir;
  Vector4 vcUp = vcWorldUp - (vcDir * angle);
  vcUp.Normalize();
  const Vector4 vcRight = vcUp.CrossProduct(vcDir);
  _11 = vcRight.x;
  _21 = vcUp.x;
  _31 = vcDir.x;
  _12 = vcRight.y;
  _22 = vcUp.y;
  _32 = vcDir.y;
  _13 = vcRight.z;
  _23 = vcUp.z;
  _33 = vcDir.z;
  _41 = vcPos.x;
  _42 = vcPos.y;
  _43 = vcPos.z;
  _14 = 0.0f;
  _24 = 0.0f;
  _34 = 0.0f;
  _44 = 1.0f;
}

inline Vector4 Matrix::operator*(const Vector4& vc) const {
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

}  // namespace render

#endif
