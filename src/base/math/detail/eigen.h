// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_BASE_MATH_DETAIL_EIGEN_H_
#define SMT_BASE_MATH_DETAIL_EIGEN_H_

#include <Eigen/Dense>
#include <Eigen/Geometry>

namespace base {

using EigenVec2 = Eigen::Matrix<float, 2, 1>;
using EigenVec3 = Eigen::Matrix<float, 3, 1>;
using EigenVec4 = Eigen::Matrix<float, 4, 1>;
using EigenMat4 = Eigen::Matrix<float, 4, 4, Eigen::RowMajor>;

}  // namespace base

#endif  // SMT_BASE_MATH_DETAIL_EIGEN_H_
