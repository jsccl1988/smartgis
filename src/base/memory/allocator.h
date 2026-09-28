// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_ALLOCATOR_H
#define BASE_MEMORY_ALLOCATOR_H

#include <stdint.h>

#include <atomic>
#include <cstddef>
#include <memory_resource>
#include <type_traits>
#include <utility>

#include "base/concurrency/queue.h"
#include "base/core/log.h"
#include "base/memory/arena.h"
#include "base/memory/memory_resource.h"
#include "base/memory/singleton.h"
#include "base/synchronization/align.h"
#include "base/traits/class_traits.h"

namespace base {
namespace detail {
template <typename Type>
inline void destruct_object(void* object) {
  reinterpret_cast<Type*>(object)->~Type();
}

template <typename Type>
inline void destruct_array(void* object, size_t count) {
  Type* ptr = reinterpret_cast<Type*>(object);
  for (size_t i = count; i > 0; --i) {
    ptr[i - 1].~Type();
  }
}

struct Deleter {
  void* p;
  size_t size;
  size_t alignment;
  bool skip_destructor;
  bool is_array;
  size_t array_count;
  void (*destructor)(void*);
  void (*array_destructor)(void*, size_t);

  struct Visitor {
    MemoryResource* memory_resource;
    void operator()(const Deleter& deleter) {
      if (!deleter.skip_destructor) {
        if (deleter.is_array && deleter.array_destructor) {
          deleter.array_destructor(deleter.p, deleter.array_count);
        } else if (deleter.destructor) {
          deleter.destructor(deleter.p);
        }
      }

      memory_resource->deallocate(deleter.p, deleter.size, deleter.alignment);
    }
  };
};

DEFINE_MEMBER_TRAITS(_DestructorSkippable);
template <typename Type>
using is_destructor_skipable = has_nested_type__DestructorSkippable<Type>;

constexpr size_t align_size(size_t size, size_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}
}  // namespace detail

// High-performance object allocator with automatic memory management
class alignas(hardware_destructive_interference_size) ObjectAllocator {
 public:
  explicit ObjectAllocator(MemoryResource* memory_resource)
      : _memory_resource(memory_resource) {
    _count.store(0, std::memory_order_relaxed);
    _total_bytes.store(0, std::memory_order_relaxed);
  }

  ObjectAllocator(const ObjectAllocator&) = delete;
  ObjectAllocator& operator=(const ObjectAllocator&) = delete;

  ObjectAllocator(ObjectAllocator&& other) noexcept
      : deleter_count_(0), _memory_resource(other._memory_resource) {
    _count.store(other._count.load(std::memory_order_relaxed),
                 std::memory_order_relaxed);
    _total_bytes.store(other._total_bytes.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
    other._count.store(0, std::memory_order_relaxed);
    other._total_bytes.store(0, std::memory_order_relaxed);
    other.deleter_count_ = 0;
  }

  ObjectAllocator& operator=(ObjectAllocator&& other) noexcept {
    if (this != &other) {
      clear(0);
      _memory_resource = other._memory_resource;
      _count.store(other._count.load(std::memory_order_relaxed),
                   std::memory_order_relaxed);
      _total_bytes.store(other._total_bytes.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
      deleter_count_ = 0;
      other._count.store(0, std::memory_order_relaxed);
      other._total_bytes.store(0, std::memory_order_relaxed);
      other.deleter_count_ = 0;
    }
    return *this;
  }

  ~ObjectAllocator() {
    LOGGING(LOG_WARNING, "<%lu, %lu, %lu> bytes",
            _count.load(std::memory_order_relaxed), _memory_resource->used(),
            _memory_resource->capacity());

    detail::Deleter::Visitor visitor{_memory_resource};
    _deleters.visit(visitor);
    _deleters.clear();
  }

  template <typename Type, typename... Args>
  [[nodiscard]] inline Type* create(Args&&... args) {
    return create_aligned<Type>(alignof(Type), std::forward<Args>(args)...);
  }

  template <typename Type, typename... Args>
  [[nodiscard]] inline Type* create_aligned(size_t alignment, Args&&... args) {
    constexpr size_t type_size = sizeof(Type);
    constexpr size_t type_align = alignof(Type);

    size_t actual_alignment = (alignment < type_align) ? type_align : alignment;
    size_t aligned_size = detail::align_size(type_size, actual_alignment);

    void* raw_ptr = _memory_resource->allocate(aligned_size, actual_alignment);
    if (!raw_ptr) {
      return nullptr;
    }

    Type* result = new (raw_ptr) Type(std::forward<Args>(args)...);
    _count.fetch_add(1, std::memory_order_relaxed);
    _total_bytes.fetch_add(aligned_size, std::memory_order_relaxed);

    constexpr bool skip_destructor =
        std::is_trivially_destructible<Type>::value ||
        detail::is_destructor_skipable<Type>::value;

    detail::Deleter deleter{raw_ptr, aligned_size, actual_alignment,
                            skip_destructor, false, 0,
                            &detail::destruct_object<Type>, nullptr};
    _deleters.push(deleter);
    deleter_count_++;

    process_deleters_batch_if_needed();

    return result;
  }

  template <typename Type, typename... Args>
  [[nodiscard]] inline Type* create_array(size_t count, Args&&... args) {
    return create_array_aligned<Type>(alignof(Type), count,
                                       std::forward<Args>(args)...);
  }

  template <typename Type, typename... Args>
  [[nodiscard]] inline Type* create_array_aligned(size_t alignment,
                                                   size_t count,
                                                   Args&&... args) {
    if (count == 0) {
      return nullptr;
    }

    constexpr size_t type_size = sizeof(Type);
    constexpr size_t type_align = alignof(Type);

    size_t actual_alignment = (alignment < type_align) ? type_align : alignment;
    size_t total_size = detail::align_size(type_size * count, actual_alignment);

    void* raw_ptr = _memory_resource->allocate(total_size, actual_alignment);
    if (!raw_ptr) {
      return nullptr;
    }

    Type* result = reinterpret_cast<Type*>(raw_ptr);
    for (size_t i = 0; i < count; ++i) {
      new (result + i) Type(args...);
    }

    _count.fetch_add(count, std::memory_order_relaxed);
    _total_bytes.fetch_add(total_size, std::memory_order_relaxed);

    constexpr bool skip_destructor =
        std::is_trivially_destructible<Type>::value ||
        detail::is_destructor_skipable<Type>::value;

    detail::Deleter deleter{raw_ptr,
                            total_size,
                            actual_alignment,
                            skip_destructor,
                            true,
                            count,
                            nullptr,
                            &detail::destruct_array<Type>};
    _deleters.push(deleter);
    deleter_count_ += count;

    process_deleters_batch_if_needed();

    return result;
  }

  [[nodiscard]] inline size_t count() const noexcept {
    return _count.load(std::memory_order_relaxed);
  }

  [[nodiscard]] inline size_t total_bytes() const noexcept {
    return _total_bytes.load(std::memory_order_relaxed);
  }

  [[nodiscard]] inline MemoryResource* memory_resource() const noexcept {
    return _memory_resource;
  }

  [[nodiscard]] inline size_t used() const noexcept {
    return _memory_resource->used();
  }

  [[nodiscard]] inline size_t capacity() const noexcept {
    return _memory_resource->capacity();
  }

  inline void clear(size_t reserved) {
    if (!_deleters.empty()) {
      detail::Deleter::Visitor visitor{_memory_resource};
      _deleters.visit(visitor);
      _deleters.clear();
    }

    _memory_resource->clear(reserved);
    _count.store(0, std::memory_order_relaxed);
    _total_bytes.store(0, std::memory_order_relaxed);
    deleter_count_ = 0;
  }

  inline void flush_deleters() {
    if (!_deleters.empty()) {
      process_deleters_batch();
      deleter_count_ = 0;
    }
  }

 private:
  static constexpr size_t kDeleterBatchSize = 100;
  mutable size_t deleter_count_{0};

  void process_deleters_batch_if_needed() {
    // Arena semantics: registered objects must stay alive until clear() or
    // ~ObjectAllocator. Eager batch destruction UAF'd still-queued thread-pool
    // PackagedTasks under load (TestContinuation.bm_* / glibc TPP crash).
    (void)kDeleterBatchSize;
  }

  void process_deleters_batch() {
    detail::Deleter::Visitor visitor{_memory_resource};
    _deleters.visit(visitor);
  }

  alignas(hardware_destructive_interference_size)
      MemoryResource* _memory_resource;
  alignas(hardware_destructive_interference_size) std::atomic<size_t> _count;
  alignas(hardware_destructive_interference_size)
      std::atomic<size_t> _total_bytes;
  PushOnlyQueue<detail::Deleter> _deleters;
};

template <typename Type, typename... Args>
[[nodiscard]] inline Type* create(Args&&... args) {
  return base::Singleton<ObjectAllocator>::instance(memory_resource())
      ->create<Type>(std::forward<Args>(args)...);
}

template <typename Type, typename... Args>
[[nodiscard]] inline Type* create_aligned(size_t alignment, Args&&... args) {
  return base::Singleton<ObjectAllocator>::instance(memory_resource())
      ->create_aligned<Type>(alignment, std::forward<Args>(args)...);
}

template <typename Type, typename... Args>
[[nodiscard]] inline Type* create_array(size_t count, Args&&... args) {
  return base::Singleton<ObjectAllocator>::instance(memory_resource())
      ->create_array<Type>(count, std::forward<Args>(args)...);
}

template <typename Type, typename... Args>
[[nodiscard]] inline Type* create_array_aligned(size_t alignment, size_t count,
                                                 Args&&... args) {
  return base::Singleton<ObjectAllocator>::instance(memory_resource())
      ->create_array_aligned<Type>(alignment, count,
                                    std::forward<Args>(args)...);
}

// STL-compatible allocator wrapper around MemoryResource
template <typename Type>
class STLAllocator : public std::pmr::polymorphic_allocator<Type> {
 public:
  using value_type = Type;
  using base_type = std::pmr::polymorphic_allocator<Type>;

  STLAllocator() : base_type(memory_resource()) {}

  explicit STLAllocator(MemoryResource* memory_resource)
      : base_type(memory_resource) {}

  template <typename U>
  STLAllocator(const std::pmr::polymorphic_allocator<U>& other)
      : base_type(other.resource()) {}

  STLAllocator(const STLAllocator& other) = default;
  STLAllocator& operator=(const STLAllocator& other) = default;

  template <typename U>
  struct rebind {
    using other = STLAllocator<U>;
  };

  bool operator==(const STLAllocator& other) const noexcept {
    return this->resource() == other.resource();
  }

  bool operator!=(const STLAllocator& other) const noexcept {
    return !(*this == other);
  }
};
}  // namespace base
#endif  // BASE_MEMORY_ALLOCATOR_H
