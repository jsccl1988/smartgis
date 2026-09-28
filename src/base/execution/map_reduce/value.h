// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_MAP_REDUCE_VALUE_H
#define BASE_EXECUTION_MAP_REDUCE_VALUE_H

#include <fstream>
#include <functional>

#include "base/container/iterator.h"

namespace base {
namespace execution {
template <typename T>
class value_iterator {
 public:
  virtual bool operator==(const value_iterator<T>& other) = 0;
  virtual bool operator!=(const value_iterator<T>& other) = 0;
  virtual void operator=(const value_iterator<T>& other) = 0;

  virtual value_iterator<T>& operator++() = 0;
  virtual void operator++(int) = 0;

  virtual const T& operator*() = 0;
  virtual const T* operator->() = 0;
};

template <typename T, typename Iter>
class value_iterator_impl : public value_iterator<T> {
 public:
  value_iterator_impl(Iter iter) : iter_(iter) {}
  virtual bool operator==(const value_iterator<T>& other) {
    const value_iterator_impl& other_cast =
        dynamic_cast<const value_iterator_impl&>(other);
    return iter_ == other_cast.iter_;
  }
  virtual bool operator!=(const value_iterator<T>& other) {
    return !((*this) == other);
  }
  virtual void operator=(const value_iterator<T>& other) {
    const value_iterator_impl& other_cast =
        dynamic_cast<const value_iterator_impl&>(other);
    iter_ = other_cast.iter_;
  }

  virtual value_iterator_impl& operator++() {
    ++iter_;
    return *this;
  }
  virtual void operator++(int) { ++(*this); }

  virtual const T& operator*() { return iter_->second; }
  virtual const T* operator->() { return &iter_->second; }

 private:
  Iter iter_;
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_MAP_REDUCE_VALUE_H
