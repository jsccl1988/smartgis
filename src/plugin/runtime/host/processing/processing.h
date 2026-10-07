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

// Worker pool for algorithm factories.
//
// Thread split (locked):
// - Compute: |factory| / |compute| runs on a pool worker. Do not touch
//   GisScene, Views, HWND, Browser*, or PluginHost present APIs
//   (gis_document / scene3d capability / playback / present_dataset).
// - Present: only from |present| (optional 5-arg submit) or the |done|
//   callback. Those run on the thread that calls flush_for_test (UI drain).
// attach_host_processing injects compute=factory(nullptr) then
// present=factory(host) on drain. Factories must skip gis_document /
// scene3d / playback / present_dataset when |host| is null.
class PLUGIN_HOST_EXPORT ProcessingPool {
 public:
  explicit ProcessingPool(ProcessingMode mode);
  ~ProcessingPool();

  ProcessingMode mode() const;
  bool submit(std::string processing_id, std::string args_json,
              content::ProcessingFactory factory,
              std::function<void(bool ok, std::string message)> done);
  // Compute on a worker; |present| then |done| on the drain / UI thread.
  // |present| is skipped when compute returns false.
  bool submit(std::string processing_id, std::string args_json,
              content::ProcessingFactory compute,
              content::ProcessingFactory present,
              std::function<void(bool ok, std::string message)> done);
  // Drain queued present/done callbacks on the caller thread (UI).
  void flush_for_test();
  // Last factory ok after the most recent flush_for_test drain.
  bool last_ok() const;
  std::string last_message() const;

 private:
  struct Job {
    std::string id;
    std::string args;
    content::ProcessingFactory factory;
    content::ProcessingFactory present;
    std::function<void(bool, std::string)> done;
  };

  void enqueue_done(std::function<void()> fn);

  ProcessingMode mode_;
  std::unique_ptr<base::execution::NThreadPoolExecutor> executor_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::vector<std::function<void()>> done_queue_;
  bool stop_ = false;
  size_t inflight_ = 0;
  bool last_ok_ = true;
  std::string last_message_;
};

// Binds host->run_processing to pool->submit without a content→plugin GN edge.
PLUGIN_HOST_EXPORT void attach_host_processing(content::PluginHost* host,
                                               ProcessingPool* pool);

}  // namespace plugin

#endif  // PLUGIN_PROCESSING_H_
