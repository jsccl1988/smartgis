// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_POOL_NUMA_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POOL_NUMA_EXECUTOR_H

#include "base/core/debug.h"
#include "base/execution/executor/pool/nthread_executor.h"
#include "base/memory/singleton.h"

#ifdef __linux__
#ifdef HAVE_NUMA
// Include NUMA headers only if HAVE_NUMA is defined
#include <numa.h>
#include <numaif.h>
#endif
#include <sched.h>
#endif

#include <atomic>
#include <vector>

namespace base {
namespace execution {
class NUMAExecutor {
 public:
  using ThreadContext = NThreadPoolExecutor::ThreadContext;

  NUMAExecutor(std::size_t threads_per_node = 0) {
#ifdef __linux__
#ifdef HAVE_NUMA
    if (numa_available() >= 0) {
      int numa_nodes = numa_max_node() + 1;
      if (numa_nodes > 0) {
        _numa_nodes = static_cast<size_t>(numa_nodes);
      } else {
        _numa_nodes = 1;
      }
    } else {
      _numa_nodes = 1;
    }
#else
    // NUMA library not available, use single node
    _numa_nodes = 1;
#endif
#else
    _numa_nodes = 1;
#endif

    if (threads_per_node == 0) {
      threads_per_node =
          ThreadContext::Thread::hardware_concurrency() / _numa_nodes;
      if (threads_per_node == 0) {
        threads_per_node = 1;
      }
    }

    _executors.reserve(_numa_nodes);
    for (size_t node = 0; node < _numa_nodes; ++node) {
      _executors.push_back(
          std::make_unique<NThreadPoolExecutor>(threads_per_node));
    }
  }

  ~NUMAExecutor() {
    for (auto &executor : _executors) {
      executor.reset();
    }
  }

  template <typename Fn, typename... Args>
  constexpr auto execute(Fn &&fn, Args &&...args) noexcept {
    size_t node = get_local_numa_node();
    return _executors[node]->execute(std::forward<Fn>(fn),
                                     std::forward<Args>(args)...);
  }

  template <typename Fn, typename... Args>
  constexpr auto execute_on_node(size_t node, Fn &&fn,
                                 Args &&...args) noexcept {
    if (node >= _numa_nodes) {
      node = 0;
    }
    return _executors[node]->execute(std::forward<Fn>(fn),
                                     std::forward<Args>(args)...);
  }

  size_t numa_nodes() const noexcept { return _numa_nodes; }

 private:
  size_t get_local_numa_node() noexcept {
#ifdef __linux__
#ifdef HAVE_NUMA
    if (numa_available() >= 0) {
      int node = numa_node_of_cpu(sched_getcpu());
      if (node >= 0 && static_cast<size_t>(node) < _numa_nodes) {
        return static_cast<size_t>(node);
      }
    }
#endif
#endif
    // Round-robin fallback
    return _current_node.fetch_add(1, std::memory_order_relaxed) % _numa_nodes;
  }

  size_t _numa_nodes;
  std::vector<std::unique_ptr<NThreadPoolExecutor>> _executors;
  std::atomic<size_t> _current_node{0};
  DISALLOW_COPY_AND_ASSIGN(NUMAExecutor);
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_POOL_NUMA_EXECUTOR_H
