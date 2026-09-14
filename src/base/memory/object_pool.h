// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_OBJECT_POOL_H_
#define BASE_MEMORY_OBJECT_POOL_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "base/core/macros.h"

namespace base {

// Thread-safe free-list pool for T (Phase 4 subset replacing SmtMemPool*).
template <typename T>
class ObjectPool {
 public:
  using value_type = std::shared_ptr<T>;
  using Factory = std::function<std::unique_ptr<T>()>;
  using Reset = std::function<void(T*)>;

  explicit ObjectPool(size_t max_size = 0, Factory factory = nullptr,
                      Reset reset = nullptr)
      : max_size_(max_size),
        factory_(std::move(factory)),
        reset_(std::move(reset)) {}

  ~ObjectPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (T* obj : objects_) {
      delete obj;
    }
    objects_.clear();
  }

  DISALLOW_COPY_AND_ASSIGN(ObjectPool);

  value_type allocate() {
    T* obj = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!objects_.empty()) {
        obj = objects_.back();
        objects_.pop_back();
        if (reset_) {
          reset_(obj);
        }
      }
    }
    if (!obj) {
      if (factory_) {
        auto up = factory_();
        obj = up.release();
      } else {
        obj = new T();
      }
    }
    return value_type(obj, [this](T* t) { recycle(t); });
  }

  size_t pool_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return objects_.size();
  }

 private:
  void recycle(T* obj) {
    if (!obj) {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (max_size_ > 0 && objects_.size() >= max_size_) {
      delete obj;
      return;
    }
    objects_.push_back(obj);
  }

  size_t max_size_;
  Factory factory_;
  Reset reset_;
  mutable std::mutex mutex_;
  std::vector<T*> objects_;
};

}  // namespace base

#endif  // BASE_MEMORY_OBJECT_POOL_H_
