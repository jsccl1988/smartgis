// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/host/processing.h"

#include <chrono>

namespace plugin {

ProcessingPool::ProcessingPool(ProcessingMode mode) : mode_(mode) {
  worker_ = std::thread([this]() { worker_main(); });
}

ProcessingPool::~ProcessingPool() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stop_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
  }
}

ProcessingMode ProcessingPool::mode() const {
  return mode_;
}

bool ProcessingPool::submit(std::string processing_id, std::string args_json,
                            content::ProcessingFactory factory,
                            std::function<void(bool ok, std::string message)> done) {
  if (processing_id.empty() || !factory) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    jobs_.push(Job{std::move(processing_id), std::move(args_json),
                   std::move(factory), std::move(done)});
  }
  cv_.notify_one();
  return true;
}

void ProcessingPool::flush_for_test() {
  for (;;) {
    std::unique_lock<std::mutex> lock(mu_);
    if (jobs_.empty() && done_queue_.empty()) {
      return;
    }
    auto dones = std::move(done_queue_);
    done_queue_.clear();
    lock.unlock();
    for (auto& fn : dones) {
      fn();
    }
    if (!dones.empty()) {
      continue;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

void ProcessingPool::worker_main() {
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [&]() { return stop_ || !jobs_.empty(); });
      if (stop_ && jobs_.empty()) {
        return;
      }
      job = std::move(jobs_.front());
      jobs_.pop();
    }
    bool ok = false;
    std::string message;
    try {
      ok = job.factory(nullptr, job.args);
    } catch (...) {
      ok = false;
      message = "factory threw";
    }
    {
      std::lock_guard<std::mutex> lock(mu_);
      done_queue_.push_back([done = std::move(job.done), ok, message]() {
        if (done) {
          done(ok, message);
        }
      });
    }
  }
}

}  // namespace plugin
