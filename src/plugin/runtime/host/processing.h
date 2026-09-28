// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PROCESSING_H_
#define PLUGIN_PROCESSING_H_

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "base/execution/executor/pool/nthread_executor.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

enum class ProcessingMode { kThread, kUtilityStub };

// Worker pool for algorithm factories. UI stays on the caller thread;
// completion callbacks are queued for flush_for_test / host drain.
class PLUGIN_HOST_EXPORT ProcessingPool {
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

  void enqueue_done(std::function<void()> fn);

  ProcessingMode mode_;
  std::unique_ptr<base::execution::NThreadPoolExecutor> executor_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::vector<std::function<void()>> done_queue_;
  bool stop_ = false;
  size_t inflight_ = 0;
};

// Binds host->run_processing to pool->submit without a content→plugin GN edge.
PLUGIN_HOST_EXPORT void attach_host_processing(content::PluginHost* host,
                                               ProcessingPool* pool);

}  // namespace plugin

#endif  // PLUGIN_PROCESSING_H_
