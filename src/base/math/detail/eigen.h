// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MATH_DETAIL_EIGEN_H_
#define BASE_MATH_DETAIL_EIGEN_H_

#include <Eigen/Dense>
#include <Eigen/Geometry>

namespace base {
namespace detail {

using EigenVec2 = Eigen::Matrix<float, 2, 1>;
using EigenVec3 = Eigen::Matrix<float, 3, 1>;
using EigenVec4 = Eigen::Matrix<float, 4, 1>;
using EigenMat4 = Eigen::Matrix<float, 4, 4, Eigen::RowMajor>;

}  // namespace detail
}  // namespace base

#endif  // BASE_MATH_DETAIL_EIGEN_H_
