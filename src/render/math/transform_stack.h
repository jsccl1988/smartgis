// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_TRANSFORM_H_
#define SMT_RENDER_MATH_TRANSFORM_H_

#include "render/math/matrix4.h"
#include "render/math/quat.h"
#include "render/math/vector.h"

#include <vector>

namespace render {

// TRS transform that can be converted to a Matrix.
class Transform {
 public:
  Vector3 translation{0, 0, 0};
  Quat rotation;
  Vector3 scale{1, 1, 1};

  Matrix matrix() const;
  void set_trs(const Vector3& t, const Quat& r, const Vector3& s) {
    translation = t;
    rotation = r;
    scale = s;
  }
};

// Matrix stack for hierarchical scene transforms (push/pop + TRS helpers).
class TransformStack {
 public:
  TransformStack() { clear(); }

  void clear() {
    stack_.clear();
    Matrix id;
    id.identity();
    stack_.push_back(id);
  }
  void push() { stack_.push_back(stack_.back()); }
  void pop() {
    if (stack_.size() > 1) {
      stack_.pop_back();
    }
  }
  void load(const Matrix& m) { stack_.back() = m; }
  void mult(const Matrix& m) { stack_.back() = stack_.back() * m; }

  void translate(float x, float y, float z) {
    Matrix t;
    t.identity();
    t.translate(x, y, z);
    mult(t);
  }
  void scale(float x, float y, float z) {
    Matrix s;
    s.identity();
    s.scale(x, y, z);
    mult(s);
  }
  void rotate_axis(const Vector4& axis, float radians) {
    Matrix r;
    r.rotate_axis(axis, radians);
    mult(r);
  }
  void rotate_quat(const Quat& q) {
    Matrix r;
    q.to_matrix(&r);
    mult(r);
  }

  const Matrix& matrix() const { return stack_.back(); }

 private:
  std::vector<Matrix> stack_;
};

inline Matrix Transform::matrix() const {
  Matrix s;
  s.identity();
  s.scale(scale.x, scale.y, scale.z);
  Matrix r;
  rotation.to_matrix(&r);
  Matrix t;
  t.identity();
  t.translate(translation.x, translation.y, translation.z);
  return t * r * s;
}

}  // namespace render

#endif  // SMT_RENDER_MATH_TRANSFORM_H_
