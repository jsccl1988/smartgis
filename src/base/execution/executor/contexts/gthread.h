// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_CONTEXTS_GTHREAD_H
#define BASE_EXECUTION_EXECUTOR_CONTEXTS_GTHREAD_H

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#include "base/concurrency/queue.h"
#include "base/core/debug.h"
#include "base/execution/executor/contexts/context.h"
#include "base/core/log.h"
#include "base/core/macros.h"
#include "base/synchronization/future.h"

// GPU support detection
#ifdef __CUDACC__
#define HAVE_CUDA 1
#elif defined(HAVE_CUDA)
#define HAVE_CUDA 1
#else
#define HAVE_CUDA 0
#endif

#if HAVE_CUDA
// Include cuda-api-wrappers (path set via config cuda include_dirs)
#include "cuda/runtime_api.hpp"
// Note: CUstream (from cuda.h via cuda-api-wrappers) and cudaStream_t
// (from cuda_runtime.h) are typically the same underlying type.
// For backward compatibility, we provide get_stream_legacy() that returns
// a type-compatible handle.
#endif

// stdgpu includes (after CUDA detection to ensure proper configuration)
// Only include stdgpu headers when CUDA is available
#if HAVE_CUDA
#include <stdgpu/execution.h>  // stdgpu::execution::device, stdgpu::execution::host
#include <stdgpu/memory.h>     // stdgpu::dynamic_memory_type, stdgpu::get_dynamic_memory_type
#include <stdgpu/device.h>     // stdgpu::print_device_information
#endif

namespace base {
namespace execution {

// Generic Thread Pool for managing parallel task execution
// Supports GPU execution when CUDA is available
class GThreadPool
    : public ThreadContext<std::mutex, std::condition_variable, std::thread> {
 public:
  explicit GThreadPool(std::size_t thread_count = 4)
      : thread_count_(thread_count),
        device_id_(0),
        should_stop_(false),
        gpu_enabled_(false) {
    initialize_gpu();
  }

  explicit GThreadPool(int device_id, std::size_t thread_count = 4)
      : thread_count_(thread_count),
        device_id_(device_id),
        should_stop_(false),
        gpu_enabled_(false) {
    initialize_gpu();
  }

  ~GThreadPool() {
    // join() already calls cleanup_gpu(), so no need to call it again
    join();
  }

  // Execute a task in the thread pool
  template <typename Fn, typename... Args>
  [[nodiscard]] constexpr auto emplace(Fn&& fn, Args&&... args) noexcept {
    auto task =
        make_twoway_task(std::forward<Fn>(fn), std::forward<Args>(args)...);
    auto future = task->get_future();
    _tasks.push(task);
    return future;
  }

  // Start worker threads
  inline void run() {
    if (!_threads.empty()) {
      LOGGING(LOG_WARNING, "Thread pool already running");
      return;
    }

    should_stop_ = false;

    // Create worker threads
    for (std::size_t i = 0; i < thread_count_; ++i) {
      _threads.emplace_back([this, i] { worker_thread(i); });
    }

    LOGGING(LOG_INFO, "Started %zu worker threads", thread_count_);
  }

  // Stop worker threads
  inline void join() {
    if (_threads.empty()) {
      // Even if no threads were started, cleanup GPU resources if they were initialized
      cleanup_gpu();
      return;
    }

    _tasks.close();
    should_stop_ = true;

    std::for_each(_threads.begin(), _threads.end(), [](Thread& thread) {
      if (thread.joinable()) {
        thread.join();
      }
    });

    _threads.clear();

    // Cleanup GPU resources after threads are joined
    cleanup_gpu();
  }

  // Get thread count
  std::size_t thread_count() const { return thread_count_; }

  // Get device ID
  int device_id() const { return device_id_; }

  // Check if GPU is enabled
  bool is_gpu_enabled() const { return gpu_enabled_; }

#if HAVE_CUDA
  // Get stdgpu device execution policy for GPU operations
  // Returns stdgpu::execution::device if GPU is enabled
  // Throws std::runtime_error if GPU is not enabled
  stdgpu::execution::device_policy get_device_policy() const {
    if (gpu_enabled_) {
      return stdgpu::execution::device;
    }
    throw std::runtime_error("GPU is not enabled, cannot return device_policy. "
                             "Check is_gpu_enabled() before calling this function.");
  }

  // Get stdgpu host execution policy for CPU operations
  stdgpu::execution::host_policy get_host_policy() const {
    return stdgpu::execution::host;
  }
#endif

#if HAVE_CUDA
  // Get CUDA stream for a specific worker thread
  // Returns the raw CUDA stream handle (CUstream) for compatibility
  // Note: CUstream is compatible with cudaStream_t (they are typically the same type)
  cuda::stream::handle_t get_stream(std::size_t worker_id) const {
    if (gpu_enabled_ && worker_id < stream_objects_.size()) {
      return stream_objects_[worker_id].handle();
    }
    return nullptr;
  }

  // Legacy compatibility: Get CUDA stream as cudaStream_t
  // This is provided for backward compatibility with code expecting cudaStream_t
  // CUstream and cudaStream_t are typically the same underlying type
  // Note: This requires cuda_runtime.h to be included elsewhere for cudaStream_t definition
  template<typename StreamType = cuda::stream::handle_t>
  StreamType get_stream_legacy(std::size_t worker_id) const {
    return static_cast<StreamType>(get_stream(worker_id));
  }

  // Get CUDA stream wrapper object for a specific worker thread
  const cuda::stream_t& get_stream_object(std::size_t worker_id) const {
    if (!gpu_enabled_ || worker_id >= stream_objects_.size()) {
      throw std::out_of_range("Invalid worker_id for stream access");
    }
    return stream_objects_[worker_id];
  }

  // Synchronize all GPU streams
  void synchronize_streams() {
#if HAVE_CUDA
    if (gpu_enabled_ && !stream_objects_.empty()) {
      for (const auto& stream : stream_objects_) {
        try {
          stream.synchronize();
        } catch (const cuda::runtime_error& e) {
          LOGGING(LOG_WARNING, "Failed to synchronize CUDA stream: %s", e.what());
        } catch (const std::exception& e) {
          LOGGING(LOG_WARNING, "Failed to synchronize CUDA stream: %s", e.what());
        }
      }
    }
#endif
  }
#endif

 private:
  // Initialize GPU context if available
  // Uses stdgpu for device management and memory type detection
  // Uses cuda-api-wrappers for CUDA API access
  // Should be called before starting worker threads
  void initialize_gpu() {
#if HAVE_CUDA
    try {
      auto device_count = cuda::device::count();
      if (device_count > 0 && device_id_ >= 0 &&
          device_id_ < static_cast<int>(device_count)) {
        // Get device and make it current
        auto device = cuda::device::get(device_id_);
        device.make_current();

        // Get device properties for logging
        auto prop = device.properties();

        // Create streams for each worker thread using cuda-api-wrappers
        // Note: stdgpu doesn't provide stream abstraction, so we use CUDA streams via cuda-api-wrappers
        stream_objects_.clear();
        stream_objects_.reserve(thread_count_);

        bool streams_created = true;
        for (std::size_t i = 0; i < thread_count_; ++i) {
          try {
            // Create a non-blocking stream (doesn't synchronize with default stream)
            // This allows concurrent execution with the default stream
            auto stream = device.create_stream(
                cuda::stream::no_implicit_synchronization_with_default_stream);
            stream_objects_.push_back(std::move(stream));
          } catch (const cuda::runtime_error& e) {
            LOGGING(LOG_WARNING, "Failed to create CUDA stream %zu: %s", i, e.what());
            streams_created = false;
            break;
          } catch (const std::exception& e) {
            LOGGING(LOG_WARNING, "Failed to create CUDA stream %zu: %s", i, e.what());
            streams_created = false;
            break;
          }
        }

        if (streams_created && stream_objects_.size() == thread_count_) {
          gpu_enabled_ = true;
          LOGGING(LOG_INFO, "GPU enabled: device %d (%s), %zu streams (cuda-api-wrappers-based)",
                  device_id_, prop.name, thread_count_);
        } else {
          // Cleanup partially created streams
          stream_objects_.clear();
          gpu_enabled_ = false;
          LOGGING(LOG_WARNING, "Failed to create all CUDA streams, GPU disabled");
        }
      } else {
        if (device_id_ < 0) {
          LOGGING(LOG_DEBUG, "Invalid device_id: %d", device_id_);
        } else {
          LOGGING(LOG_DEBUG, "No CUDA devices available or invalid device_id: %d (available: %zu)",
                  device_id_, device_count);
        }
        gpu_enabled_ = false;
      }
    } catch (const cuda::runtime_error& e) {
      LOGGING(LOG_DEBUG, "CUDA not available: %s", e.what());
      gpu_enabled_ = false;
    } catch (const std::exception& e) {
      LOGGING(LOG_DEBUG, "Failed to initialize CUDA: %s", e.what());
      gpu_enabled_ = false;
    } catch (...) {
      LOGGING(LOG_DEBUG, "Failed to initialize CUDA: unknown error");
      gpu_enabled_ = false;
    }
#else
    // No CUDA support compiled in
    gpu_enabled_ = false;
#endif
  }

  // Cleanup GPU resources
  // Should be called after all worker threads are joined
  // Safe to call multiple times (idempotent)
  void cleanup_gpu() {
#if HAVE_CUDA
    if (gpu_enabled_ || !stream_objects_.empty()) {
      try {
        synchronize_streams();
      } catch (...) {
        // Ignore exceptions during cleanup
      }
      // Streams will be automatically destroyed when stream_objects_ is cleared
      // (RAII via cuda-api-wrappers)
      stream_objects_.clear();
      gpu_enabled_ = false;
    }
#else
    // No CUDA support, ensure gpu_enabled_ is false
    gpu_enabled_ = false;
#endif
  }

  // Worker thread function
  // Tasks executed here can use stdgpu execution policies and memory management
  inline void worker_thread(std::size_t worker_id) {
#if HAVE_CUDA
    // Set GPU device context for this thread
    if (gpu_enabled_) {
      try {
        auto device = cuda::device::get(device_id_);
        device.make_current();
        // Each worker thread can use its dedicated stream
        // Access via get_stream(worker_id) or get_stream_object(worker_id) if needed
        // Tasks can use stdgpu::execution::device for GPU operations
      } catch (const cuda::runtime_error& e) {
        LOGGING(LOG_WARNING, "Worker thread %zu failed to set CUDA device %d: %s",
                worker_id, device_id_, e.what());
      }
    }
#endif

    while (true) {
      base::execution::Task* task = nullptr;
      if (LIKELY(_tasks.pop(task))) {
        // Execute task
        // Tasks can use:
        // - get_stream(worker_id) to get their CUDA stream
        // - stdgpu::execution::device for GPU execution policies
        // - stdgpu memory management APIs (createDeviceArray, etc.)
        try {
          (*task)();
        } catch (...) {
          LOGGING(LOG_ERROR, "Exception in worker thread %zu task execution", worker_id);
          // Continue processing other tasks
        }
        delete task;
      } else {
        break;
      }
    }
  }

  std::size_t thread_count_;
  int device_id_;
  std::vector<Thread> _threads;
  TaskQueue<base::execution::Task*, Mutex, ConditionVariable> _tasks;
  bool should_stop_;
  bool gpu_enabled_;

#if HAVE_CUDA
  std::vector<cuda::stream_t> stream_objects_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GThreadPool);
};

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTOR_CONTEXTS_GTHREAD_H
