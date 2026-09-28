// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_SINGLETON_H
#define BASE_MEMORY_SINGLETON_H

#include <memory>

#include "base/core/macros.h"

namespace base {
template <typename T>
class Singleton {
 public:
  template <typename... Args>
  static T* instance(Args&&... args) {
    static T t(std::forward<Args>(args)...);
    return &t;
  }
  DISALLOW_COPY_AND_ASSIGN(Singleton);
};

template <char* kNamed, typename T>
class NamedSingleton {
 public:
  template <typename... Args>
  static T* instance(Args&&... args) {
    static T t(std::forward<Args>(args)...);
    return &t;
  }
  DISALLOW_COPY_AND_ASSIGN(NamedSingleton);
};

template <typename T>
class ThreadLocalSingleton {
 public:
  template <typename... Args>
  static T* instance(Args&&... args) {
    static thread_local std::unique_ptr<T> t =
        std::make_unique<T>(std::forward<Args>(args)...);
    return t.get();
  }
  DISALLOW_COPY_AND_ASSIGN(ThreadLocalSingleton);
};

}  // namespace base
#endif  // BASE_MEMORY_SINGLETON_H
