// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/memory/allocation_tracker.h"
#include "base/memory/allocator.h"
#include "base/memory/arena.h"
#include "base/memory/object_pool.h"
#include "base/memory/scope_guard.h"
#include "base/memory/singleton.h"

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct PooledInt {
  int value = 0;
  using _DestructorSkippable = void;
};

bool flame_has_type(const base::AllocationTracker::FlameNode& node,
                    const char* needle) {
  if (node.type && std::string(node.type).find(needle) != std::string::npos) {
    return true;
  }
  for (const base::AllocationTracker::FlameNode& child : node.children) {
    if (flame_has_type(child, needle)) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  {
    base::Arena arena(base::MemoryResource::Type::kMonotonicBuffer, 64 * 1024);
    void* p = arena.memory_resource->allocate(128, 8);
    assert(p != nullptr);
    std::memset(p, 0xab, 128);
    arena.memory_resource->clear(64 * 1024);
  }

  {
    void* p = base::allocate(256);
    assert(p != nullptr);
    base::deallocate(p, 256);
  }

  {
    base::ObjectPool<PooledInt> pool(4);
    auto a = pool.allocate();
    a->value = 7;
    assert(a->value == 7);
    a.reset();
    auto b = pool.allocate();
    assert(b);
    const auto stats = pool.get_statistics();
    assert(stats.total_created >= 1u);
  }

  {
    int* n = base::create<int>(42);
    assert(n && *n == 42);
  }

  {
    struct FlameProbe {
      int n = 1;
    };
    base::AllocationTracker::reset();
    base::AllocationTracker::enable();
    FlameProbe* probe = base::create<FlameProbe>();
    assert(probe && probe->n == 1);
    const auto tree = base::AllocationTracker::flame_snapshot();
    assert(!tree.empty());
    assert(tree.front().bytes >= sizeof(FlameProbe));
    assert(flame_has_type(tree.front(), "FlameProbe"));
    base::AllocationTracker::disable();
    base::AllocationTracker::reset();
  }

  {
    int fired = 0;
    {
      ON_SCOPE_EXIT {
        fired = 1;
      };
      assert(fired == 0);
    }
    assert(fired == 1);
  }

  {
    auto* s = base::Singleton<std::string>::instance("ok");
    assert(s && *s == "ok");
  }

  {
    base::MemoryResource* mr =
        base::MemoryResource::create(base::MemoryResource::Type::kHybridOptimized,
                                     256 * 1024);
    assert(mr);
    void* p = mr->allocate(32, 8);
    assert(p);
    mr->deallocate(p, 32, 8);
    base::MemoryResource::destroy(mr);
  }

  {
    std::vector<int, base::STLAllocator<int>> v;
    v.push_back(1);
    v.push_back(2);
    assert(v.size() == 2u);
  }

  return 0;
}
