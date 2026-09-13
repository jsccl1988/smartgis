// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PROCESSING_H_
#define PLUGIN_PROCESSING_H_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "content/public/plugin_host.h"

namespace plugin {

enum class ProcessingMode { kThread, kUtilityStub };

// One worker thread for algorithm factories. UI stays on the caller thread.
class ProcessingPool {
 public:
  explicit ProcessingPool(ProcessingMode mode);
  ~ProcessingPool();

  ProcessingMode mode() const;
  bool submit(std::string processing_id, std::string args_json,
              content::ProcessingFactory factory,
              std::function<void(bool ok, std::string message)> done);
  void flush_for_test();

 private:
  struct Job {
    std::string id;
    std::string args;
    content::ProcessingFactory factory;
    std::function<void(bool, std::string)> done;
  };

  void worker_main();

  ProcessingMode mode_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::queue<Job> jobs_;
  std::vector<std::function<void()>> done_queue_;
  bool stop_ = false;
  std::thread worker_;
};

// Binds host->run_processing to pool->submit without a content→plugin GN edge.
void attach_host_processing(content::PluginHost* host, ProcessingPool* pool);

}  // namespace plugin

#endif  // PLUGIN_PROCESSING_H_
