// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/trace/diagnostic_bootstrap.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "base/core/log.h"
#include "base/memory/allocation_tracker.h"
#include "base/memory/sample_trace.h"
#include "base/trace/process_trace.h"

namespace base {
namespace {

std::mutex& bootstrap_mu() {
  static std::mutex mu;
  return mu;
}

std::atomic<bool>& sampler_run() {
  static std::atomic<bool> run{false};
  return run;
}

std::thread& sampler_thread() {
  static std::thread th;
  return th;
}

std::atomic<bool>& started_flag() {
  static std::atomic<bool> started{false};
  return started;
}

void sampler_loop() {
  while (sampler_run().load(std::memory_order_relaxed)) {
    sample_memory_counters_to_process_trace();
    for (int i = 0; i < 50 && sampler_run().load(std::memory_order_relaxed);
         ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
}

}  // namespace

void start_always_on_diagnostics() {
  std::lock_guard<std::mutex> lock(bootstrap_mu());
  if (started_flag().load(std::memory_order_relaxed)) {
    return;
  }

  AllocationTracker::enable();
  if (!tracing_enabled()) {
    set_tracing_enabled(true);
  }

  sampler_run().store(true, std::memory_order_relaxed);
  sampler_thread() = std::thread(sampler_loop);
  started_flag().store(true, std::memory_order_relaxed);

  LOGGING(LOG_INFO, "diagnostics: always-on tracing + alloc tracker + memory "
                    "sampler (500ms) started");
  sample_memory_counters_to_process_trace();
}

void stop_always_on_diagnostics() {
  std::lock_guard<std::mutex> lock(bootstrap_mu());
  if (!started_flag().load(std::memory_order_relaxed)) {
    return;
  }
  sampler_run().store(false, std::memory_order_relaxed);
  if (sampler_thread().joinable()) {
    sampler_thread().join();
  }
  started_flag().store(false, std::memory_order_relaxed);
  LOGGING(LOG_INFO, "diagnostics: memory sampler stopped");
}

bool always_on_diagnostics_started() {
  return started_flag().load(std::memory_order_relaxed);
}

}  // namespace base
