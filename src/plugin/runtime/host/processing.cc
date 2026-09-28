// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/processing.h"

#include <chrono>
#include <utility>

#include "plugin/runtime/host/operation_result.h"

namespace plugin {

ProcessingPool::ProcessingPool(ProcessingMode mode) : mode_(mode) {
  const std::size_t workers =
      mode_ == ProcessingMode::kUtilityStub
          ? 1
          : base::execution::NThreadPool::Thread::hardware_concurrency();
  executor_ = std::make_unique<base::execution::NThreadPoolExecutor>(
      workers == 0 ? 1 : workers);
}

ProcessingPool::~ProcessingPool() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stop_ = true;
  }
  cv_.notify_all();
  // Drain in-flight work by waiting until inflight_ hits zero.
  std::unique_lock<std::mutex> lock(mu_);
  cv_.wait(lock, [&]() { return inflight_ == 0; });
  executor_.reset();
}

ProcessingMode ProcessingPool::mode() const {
  return mode_;
}

void ProcessingPool::enqueue_done(std::function<void()> fn) {
  std::lock_guard<std::mutex> lock(mu_);
  done_queue_.push_back(std::move(fn));
  --inflight_;
  cv_.notify_all();
}

bool ProcessingPool::submit(
    std::string processing_id, std::string args_json,
    content::ProcessingFactory factory,
    std::function<void(bool ok, std::string message)> done) {
  if (processing_id.empty() || !factory || !executor_ || stop_) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stop_) {
      return false;
    }
    ++inflight_;
  }
  Job job{std::move(processing_id), std::move(args_json), std::move(factory),
          std::move(done)};
  (void)executor_->execute([this, job = std::move(job)]() mutable {
    bool ok = false;
    std::string message;
    set_operation_result("");
    try {
      ok = job.factory(nullptr, job.args);
      message = operation_result();
    } catch (...) {
      ok = false;
      message = "factory threw";
      set_operation_result(message);
    }
    enqueue_done([done = std::move(job.done), ok,
                  message = std::move(message)]() {
      if (done) {
        done(ok, message);
      }
    });
  });
  return true;
}

void ProcessingPool::flush_for_test() {
  for (;;) {
    std::unique_lock<std::mutex> lock(mu_);
    if (inflight_ == 0 && done_queue_.empty()) {
      return;
    }
    auto dones = std::move(done_queue_);
    done_queue_.clear();
    const bool waiting = inflight_ > 0 && dones.empty();
    if (waiting) {
      cv_.wait_for(lock, std::chrono::milliseconds(1));
      continue;
    }
    lock.unlock();
    for (auto& fn : dones) {
      fn();
    }
  }
}

void attach_host_processing(content::PluginHost* host, ProcessingPool* pool) {
  if (!host || !pool) {
    return;
  }
  host->set_processing_pool(pool);
  host->set_processing_enqueue(
      [host, pool](std::string processing_id, std::string args_json,
                   content::ProcessingFactory factory) {
        return pool->submit(
            std::move(processing_id), std::move(args_json),
            [host, factory](content::PluginHost*, std::string_view args) {
              return factory(host, args);
            },
            [](bool, std::string) {});
      });
}

}  // namespace plugin
