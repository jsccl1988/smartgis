// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/processing/processing.h"

#include "plugin/runtime/host/processing/operation_result.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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

bool ProcessingPool::last_ok() const {
  std::lock_guard<std::mutex> lock(mu_);
  return last_ok_;
}

std::string ProcessingPool::last_message() const {
  std::lock_guard<std::mutex> lock(mu_);
  return last_message_;
}

bool ProcessingPool::submit(
    std::string processing_id, std::string args_json,
    content::ProcessingFactory factory,
    std::function<void(bool ok, std::string message)> done) {
  return submit(std::move(processing_id), std::move(args_json),
                std::move(factory), content::ProcessingFactory{},
                std::move(done));
}

bool ProcessingPool::submit(
    std::string processing_id, std::string args_json,
    content::ProcessingFactory compute, content::ProcessingFactory present,
    std::function<void(bool ok, std::string message)> done) {
  if (processing_id.empty() || !compute || !executor_ || stop_) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stop_) {
      return false;
    }
    ++inflight_;
  }
  Job job{std::move(processing_id), std::move(args_json), std::move(compute),
          std::move(present), std::move(done)};
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
    enqueue_done([this, present = std::move(job.present),
                  args = std::move(job.args), done = std::move(job.done), ok,
                  message = std::move(message)]() mutable {
      if (ok && present) {
        try {
          if (!present(nullptr, args)) {
            ok = false;
            const std::string present_msg = operation_result();
            if (!present_msg.empty()) {
              message = present_msg;
            }
          }
        } catch (...) {
          ok = false;
          message = "present threw";
        }
      }
      {
        std::lock_guard<std::mutex> lock(mu_);
        last_ok_ = ok;
        last_message_ = message;
      }
      if (done) {
        done(ok, message);
      }
    });
  });
  return true;
}

void ProcessingPool::flush_for_test() {
  for (;;) {
    std::vector<std::function<void()>> dones;
    {
      std::unique_lock<std::mutex> lock(mu_);
      if (inflight_ == 0 && done_queue_.empty()) {
        return;
      }
      dones = std::move(done_queue_);
      done_queue_.clear();
      if (inflight_ > 0 && dones.empty()) {
        lock.unlock();
        // Showcase callers block on this from the UI thread. Worker factories
        // publish into Scene3d / MapScene which needs DispatchMessage (display
        // present, HWND) — wait without pumping deadlocks until suite timeout.
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
          if (msg.message == WM_QUIT) {
            return;
          }
          TranslateMessage(&msg);
          DispatchMessageW(&msg);
        }
        Sleep(1);
        continue;
      }
    }
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
        // Compute with a null host so factories skip gis_document /
        // present_dataset / chrome writers. Present re-enters with the real
        // host on the flush / UI drain thread (ProcessingPool 5-arg submit).
        return pool->submit(
            std::move(processing_id), std::move(args_json),
            [factory](content::PluginHost*, std::string_view args) {
              return factory(nullptr, args);
            },
            [host, factory](content::PluginHost*, std::string_view args) {
              return factory(host, args);
            },
            [](bool, std::string) {});
      });
}

}  // namespace plugin
