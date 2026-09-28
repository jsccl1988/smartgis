// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_NONCOPYABLE_H
#define BASE_MEMORY_NONCOPYABLE_H

namespace base {
class noncopyable {
  noncopyable(noncopyable&) = delete;
  noncopyable(const noncopyable&) = delete;
  noncopyable& operator=(noncopyable&) = delete;
  noncopyable& operator=(const noncopyable&) = delete;

 public:
  noncopyable() = default;
  ~noncopyable() = default;
  noncopyable(noncopyable&&) = default;
  noncopyable& operator=(noncopyable&&) = default;
};
}  // namespace base
#endif  // BASE_MEMORY_NONCOPYABLE_H