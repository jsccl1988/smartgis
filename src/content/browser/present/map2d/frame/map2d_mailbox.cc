// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_mailbox.h"

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/futures/combinators/async.h"

namespace content {
namespace detail {

void Map2dMailbox::mark_idle_and_notify() {
  inflight_ = false;
  cv_.notify_all();
}

void Map2dMailbox::cancel_and_wait(std::unique_lock<std::mutex>& lock) {
  request_gen_.fetch_add(1, std::memory_order_acq_rel);
  while (inflight_) {
    cv_.wait(lock);
  }
}

void Map2dMailbox::post_drain(std::unique_lock<std::mutex>& lock,
                              Map2dFrameCache* cache) {
  inflight_ = true;
  lock.unlock();
  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::async(executor, [cache]() { cache->drain_layout_jobs(); });
  lock.lock();
}

}  // namespace detail
}  // namespace content
