// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_ARENA_H_
#define BASE_MEMORY_ARENA_H_

#include <stdint.h>

#include <atomic>
#include <cstddef>
#include <memory_resource>
#include <type_traits>
#include <cstring>
#include <utility>

#include "base/core/log.h"
#include "base/memory/memory_resource.h"
#include "base/memory/singleton.h"

namespace base {
struct Arena {
  static constexpr int32_t kInitialSize = 4 * 1024 * 1024;
  using unique_memory_resource_t =
      std::unique_ptr<MemoryResource, decltype(&MemoryResource::destroy)>;
  unique_memory_resource_t memory_resource;

  Arena(MemoryResource::Type type = MemoryResource::Type::kSynchronizedPool,
        size_t size = kInitialSize)
      : memory_resource(MemoryResource::create(type, size),
                        &MemoryResource::destroy) {}
};

inline MemoryResource* memory_resource() {
  static auto memory_resource =
      base::Singleton<Arena>::instance()->memory_resource.get();
  return memory_resource;
}

inline MemoryResource* tls_memory_resource() {
  thread_local static Arena tls_arena(MemoryResource::Type::kHybridOptimized,
                                      Arena::kInitialSize);
  return tls_arena.memory_resource.get();
}

[[nodiscard]] inline void* allocate(
    size_t bytes, size_t alignment = MemoryResource::kDefaultAlignment) {
  return memory_resource()->allocate(bytes, alignment);
}

[[nodiscard]] inline void* tls_allocate(
    size_t bytes, size_t alignment = MemoryResource::kDefaultAlignment) {
  return tls_memory_resource()->allocate(bytes, alignment);
}

inline void deallocate(void* p, size_t bytes,
                       size_t alignment = MemoryResource::kDefaultAlignment) {
  memory_resource()->deallocate(p, bytes, alignment);
}

inline void tls_deallocate(void* p, size_t bytes,
                           size_t alignment = MemoryResource::kDefaultAlignment) {
  tls_memory_resource()->deallocate(p, bytes, alignment);
}

[[nodiscard]] inline void* realloc(
    void* old_data, size_t old_size, size_t new_size,
    size_t alignment = MemoryResource::kDefaultAlignment) {
  if (old_size >= new_size) {
    return old_data;
  }

  void* new_data = memory_resource()->allocate(new_size, alignment);
  if (new_data != nullptr) {
    std::memmove(new_data, old_data, old_size);
    memory_resource()->deallocate(old_data, old_size, alignment);
    return new_data;
  }

  return nullptr;
}

[[nodiscard]] inline void* tls_realloc(
    void* old_data, size_t old_size, size_t new_size,
    size_t alignment = MemoryResource::kDefaultAlignment) {
  if (old_size >= new_size) {
    return old_data;
  }

  MemoryResource* mr = tls_memory_resource();
  void* new_data = mr->allocate(new_size, alignment);
  if (new_data != nullptr) {
    std::memmove(new_data, old_data, old_size);
    mr->deallocate(old_data, old_size, alignment);
    return new_data;
  }

  return nullptr;
}

template <size_t Size>
constexpr size_t align_size() {
  if constexpr (Size <= 8) return 8;
  else if constexpr (Size <= 16) return 16;
  else if constexpr (Size <= 32) return 32;
  else return 64;
}

template <size_t Alignment>
[[nodiscard]] inline void* allocate_aligned(size_t bytes) {
  static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be power of 2");
  return allocate(bytes, Alignment);
}

template <size_t Alignment>
[[nodiscard]] inline void* tls_allocate_aligned(size_t bytes) {
  static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be power of 2");
  return tls_allocate(bytes, Alignment);
}

template <size_t Alignment>
[[nodiscard]] inline void* realloc_aligned(
    void* old_data, size_t old_size, size_t new_size) {
  return realloc(old_data, old_size, new_size, Alignment);
}

template <size_t Alignment>
[[nodiscard]] inline void* tls_realloc_aligned(
    void* old_data, size_t old_size, size_t new_size) {
  return tls_realloc(old_data, old_size, new_size, Alignment);
}

[[nodiscard]] inline void* allocate_simd(size_t bytes) {
  return allocate_aligned<16>(bytes);
}

[[nodiscard]] inline void* tls_allocate_simd(size_t bytes) {
  return tls_allocate_aligned<16>(bytes);
}

[[nodiscard]] inline void* allocate_simd_256(size_t bytes) {
  return allocate_aligned<32>(bytes);
}

[[nodiscard]] inline void* tls_allocate_simd_256(size_t bytes) {
  return tls_allocate_aligned<32>(bytes);
}
}  // namespace base
#endif  // BASE_MEMORY_ARENA_H_
