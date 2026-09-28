// Copyright (c) 2023 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CONCURRENCY_DEBUGGER_H
#define BASE_CONCURRENCY_DEBUGGER_H

#ifdef ENABLE_DEBUG

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace base {

// Helper: Get current timestamp in nanoseconds
inline std::uint64_t get_timestamp_ns() {
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

// Information about a held lock
struct LockInfo {
  void* lock_address;
  std::thread::id thread_id;
  std::string lock_name;
  std::size_t acquisition_order;
  std::uint64_t timestamp_ns;

  LockInfo(void* addr, std::thread::id tid, const std::string& name,
           std::size_t order)
      : lock_address(addr),
        thread_id(tid),
        lock_name(name),
        acquisition_order(order),
        timestamp_ns(get_timestamp_ns()) {}
};

// Information about memory access for race condition detection
struct MemoryAccessInfo {
  void* address;
  std::thread::id thread_id;
  bool is_write;
  std::uint64_t timestamp_ns;
  std::string location;

  MemoryAccessInfo(void* addr, std::thread::id tid, bool write,
                   const std::string& loc)
      : address(addr),
        thread_id(tid),
        is_write(write),
        timestamp_ns(get_timestamp_ns()),
        location(loc) {}
};

// Concurrency debugger for deadlock and race condition detection
class ConcurrencyDebugger {
 public:
  static void on_lock_acquired(void* lock_address,
                               const std::string& lock_name = "") {
    if (!is_enabled()) return;

    auto thread_id = std::this_thread::get_id();
    auto order = acquisition_order_counter_.fetch_add(1, std::memory_order_relaxed);
    auto name = lock_name.empty() ? format_lock_name(lock_address) : lock_name;

    LockInfo info(lock_address, thread_id, name, order);
    held_locks_.push_back(info);

    {
      std::lock_guard<std::mutex> lock(global_mutex_);
      lock_registry_[lock_address] = info;
    }

    check_deadlock();
  }

  static void on_lock_released(void* lock_address) {
    if (!is_enabled()) return;

    auto& locks = held_locks_;
    locks.erase(
        std::remove_if(locks.begin(), locks.end(),
                       [lock_address](const LockInfo& info) {
                         return info.lock_address == lock_address;
                       }),
        locks.end());

    {
      std::lock_guard<std::mutex> lock(global_mutex_);
      lock_registry_.erase(lock_address);
    }
  }

  static void on_memory_access(void* address, bool is_write,
                               const std::string& location = "") {
    if (!is_enabled()) return;

    MemoryAccessInfo access(address, std::this_thread::get_id(), is_write,
                            location.empty() ? "unknown" : location);
    detect_race_condition(access);
  }

  static void check_deadlock() {
    if (!is_enabled()) return;

    std::lock_guard<std::mutex> lock(global_mutex_);

    // Build wait-for graph: thread A -> thread B means A waits for lock held by B
    std::unordered_map<std::thread::id, std::unordered_set<std::thread::id>> wait_graph;
    auto current_thread_id = std::this_thread::get_id();

    // Current thread holds locks in held_locks_, check if it's waiting for any
    for (const auto& held_lock : held_locks_) {
      for (const auto& [lock_addr, lock_info] : lock_registry_) {
        if (lock_info.thread_id != current_thread_id &&
            lock_info.thread_id != held_lock.thread_id) {
          // Current thread might wait for lock held by lock_info.thread_id
          wait_graph[current_thread_id].insert(lock_info.thread_id);
        }
      }
    }

    // Detect cycles using DFS
    std::unordered_set<std::thread::id> visited, rec_stack;
    std::vector<std::thread::id> cycle;

    for (const auto& [thread_id, _] : wait_graph) {
      if (visited.find(thread_id) == visited.end()) {
        if (has_cycle(thread_id, wait_graph, visited, rec_stack, cycle)) {
          report_deadlock(cycle);
          break;
        }
      }
    }
  }

  static void detect_race_condition(const MemoryAccessInfo& access) {
    if (!is_enabled()) return;

    std::lock_guard<std::mutex> lock(global_mutex_);

    auto it = memory_accesses_.find(access.address);
    if (it != memory_accesses_.end()) {
      const auto& prev = it->second;

      // Race condition: different threads, at least one write, concurrent access
      if (prev.thread_id != access.thread_id &&
          (prev.is_write || access.is_write)) {
        auto time_diff = (access.timestamp_ns > prev.timestamp_ns)
                            ? (access.timestamp_ns - prev.timestamp_ns)
                            : (prev.timestamp_ns - access.timestamp_ns);

        constexpr std::uint64_t RACE_THRESHOLD_NS = 1000;  // 1 microsecond
        if (time_diff < RACE_THRESHOLD_NS) {
          report_race_condition(prev, access);
        }
      }
    }

    memory_accesses_[access.address] = access;
  }

  static void set_enabled(bool enabled) {
    enabled_.store(enabled, std::memory_order_release);
  }

  static bool is_enabled() {
    return enabled_.load(std::memory_order_acquire);
  }

  static std::vector<LockInfo> get_held_locks() {
    return held_locks_;
  }

  static void clear() {
    std::lock_guard<std::mutex> lock(global_mutex_);
    lock_registry_.clear();
    memory_accesses_.clear();
    // Note: held_locks_ is thread_local, each thread clears its own
  }

 private:
  static std::string format_lock_name(void* addr) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "lock_%p", addr);
    return std::string(buffer);
  }

  static bool has_cycle(
      std::thread::id node,
      const std::unordered_map<std::thread::id,
                               std::unordered_set<std::thread::id>>& graph,
      std::unordered_set<std::thread::id>& visited,
      std::unordered_set<std::thread::id>& rec_stack,
      std::vector<std::thread::id>& cycle) {
    visited.insert(node);
    rec_stack.insert(node);
    cycle.push_back(node);

    auto it = graph.find(node);
    if (it != graph.end()) {
      for (const auto& neighbor : it->second) {
        if (visited.find(neighbor) == visited.end()) {
          if (has_cycle(neighbor, graph, visited, rec_stack, cycle)) {
            return true;
          }
        } else if (rec_stack.find(neighbor) != rec_stack.end()) {
          // Found cycle: trim to start from neighbor
          auto start = std::find(cycle.begin(), cycle.end(), neighbor);
          if (start != cycle.end()) {
            cycle.erase(cycle.begin(), start);
            cycle.push_back(neighbor);
          }
          return true;
        }
      }
    }

    rec_stack.erase(node);
    cycle.pop_back();
    return false;
  }

  static void report_deadlock(const std::vector<std::thread::id>& cycle) {
    // Note: caller already holds global_mutex_
    std::cerr << "[DEADLOCK DETECTED] Cycle in lock acquisition:\n";
    for (std::size_t i = 0; i < cycle.size(); ++i) {
      std::cerr << "  Thread " << cycle[i];
      if (i < cycle.size() - 1) std::cerr << " -> ";
    }
    std::cerr << "\n";

    for (const auto& [addr, info] : lock_registry_) {
      for (const auto& tid : cycle) {
        if (info.thread_id == tid) {
          std::cerr << "  Lock " << info.lock_name << " (" << addr
                    << ") held by thread " << tid << "\n";
        }
      }
    }
  }

  static void report_race_condition(const MemoryAccessInfo& a1,
                                    const MemoryAccessInfo& a2) {
    std::cerr << "[RACE CONDITION] Concurrent access to " << a1.address << ":\n";
    std::cerr << "  Thread " << a1.thread_id << " "
              << (a1.is_write ? "wrote" : "read") << " at " << a1.location
              << " (time: " << a1.timestamp_ns << " ns)\n";
    std::cerr << "  Thread " << a2.thread_id << " "
              << (a2.is_write ? "wrote" : "read") << " at " << a2.location
              << " (time: " << a2.timestamp_ns << " ns)\n";
  }

  static thread_local std::vector<LockInfo> held_locks_;
  static std::unordered_map<void*, LockInfo> lock_registry_;
  static std::unordered_map<void*, MemoryAccessInfo> memory_accesses_;
  static std::mutex global_mutex_;
  static std::atomic<bool> enabled_;
  static std::atomic<std::size_t> acquisition_order_counter_;
};

// Static member definitions
thread_local std::vector<LockInfo> ConcurrencyDebugger::held_locks_;
std::unordered_map<void*, LockInfo> ConcurrencyDebugger::lock_registry_;
std::unordered_map<void*, MemoryAccessInfo> ConcurrencyDebugger::memory_accesses_;
std::mutex ConcurrencyDebugger::global_mutex_;
std::atomic<bool> ConcurrencyDebugger::enabled_{false};
std::atomic<std::size_t> ConcurrencyDebugger::acquisition_order_counter_{0};

}  // namespace base

#endif  // ENABLE_DEBUG

#endif  // BASE_CONCURRENCY_DEBUGGER_H

