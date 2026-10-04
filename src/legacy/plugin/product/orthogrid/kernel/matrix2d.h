// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_MATRIX2D_H_
#define PLUGIN_ORTHOGRID_MATRIX2D_H_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace geo {

// Leftover-owned dense row-major 2D array for orthogrid numeric buffers.
// Not product Geometry and not plugin OrthoLattice. Namespace geo is kept so leftover
// `using geo::Matrix2D` stays a one-include change.
template <typename T>
class Matrix2D {
 public:
  Matrix2D() = default;
  Matrix2D(int n_row, int n_col) { resize(n_row, n_col); }
  ~Matrix2D() = default;

  Matrix2D(const Matrix2D&) = default;
  Matrix2D& operator=(const Matrix2D&) = default;
  Matrix2D(Matrix2D&&) noexcept = default;
  Matrix2D& operator=(Matrix2D&&) noexcept = default;

  void resize(int n_row, int n_col) {
    assert(n_row > 0 && n_col > 0);
    if (n_row == row_ && n_col == col_) {
      return;
    }
    row_ = n_row;
    col_ = n_col;
    data_.assign(static_cast<size_t>(row_) * static_cast<size_t>(col_), T{});
  }

  void clear() {
    data_.clear();
    row_ = 0;
    col_ = 0;
  }

  int row_count() const { return row_; }
  int col_count() const { return col_; }
  int size() const { return row_ * col_; }
  bool empty() const { return data_.empty(); }

  void calc_index(int& i_row, int& j_col, int index) const {
    assert(col_ > 0);
    i_row = index / col_;
    j_col = index % col_;
  }

  int calc_index(int i_row, int j_col) const { return col_ * i_row + j_col; }

  void set_element(const T& value, int i_row, int j_col) {
    data_[static_cast<size_t>(index_of(i_row, j_col))] = value;
  }

  const T& get_element(int i_row, int j_col) const {
    return data_[static_cast<size_t>(index_of(i_row, j_col))];
  }

  T& get_element(int i_row, int j_col) {
    return data_[static_cast<size_t>(index_of(i_row, j_col))];
  }

  void set_elements(std::span<const T> src) {
    assert(static_cast<int>(src.size()) == size());
    std::copy(src.begin(), src.end(), data_.begin());
  }

  void get_elements(std::span<T> dst) const {
    assert(static_cast<int>(dst.size()) == size());
    std::copy(data_.begin(), data_.end(), dst.begin());
  }

  std::span<T> elements() { return data_; }
  std::span<const T> elements() const { return data_; }

  T* operator[](int i_row) {
    assert(i_row >= 0 && i_row < row_);
    return data_.data() + static_cast<size_t>(i_row) * static_cast<size_t>(col_);
  }

  const T* operator[](int i_row) const {
    assert(i_row >= 0 && i_row < row_);
    return data_.data() + static_cast<size_t>(i_row) * static_cast<size_t>(col_);
  }

  T& operator()(int i_row, int j_col) { return get_element(i_row, j_col); }

  const T& operator()(int i_row, int j_col) const {
    return get_element(i_row, j_col);
  }

 private:
  int index_of(int i_row, int j_col) const {
    assert(i_row >= 0 && j_col >= 0 && i_row < row_ && j_col < col_);
    return col_ * i_row + j_col;
  }

  int row_ = 0;
  int col_ = 0;
  std::vector<T> data_;
};

}  // namespace geo

#endif  // PLUGIN_ORTHOGRID_MATRIX2D_H_
