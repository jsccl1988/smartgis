// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_MAILBOX_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_MAILBOX_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace content {

class Map2dFrameCache;

namespace detail {

// E-lane: request generation, inflight flag, and the cv used for export /
// first-present wait. The pending LayoutJob payload stays on the cache.
// Does not own mu_ — post_drain unlocks before async.
struct Map2dMailbox {
  uint64_t bump_gen() {
    return request_gen_.fetch_add(1, std::memory_order_acq_rel) + 1;
  }
  uint64_t request_gen() const {
    return request_gen_.load(std::memory_order_acquire);
  }
  const std::atomic<uint64_t>* live_layout_gen() const { return &request_gen_; }
  bool inflight() const { return inflight_; }
  void mark_inflight() { inflight_ = true; }
  void mark_idle_and_notify();

  void cancel_and_wait(std::unique_lock<std::mutex>& lock);

  // Unlock, post drain on the sole pool, relock. Drain must not nest mu_.
  void post_drain(std::unique_lock<std::mutex>& lock, Map2dFrameCache* cache);

  std::atomic<uint64_t> request_gen_{0};
  bool inflight_ = false;
  std::condition_variable cv_;
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_MAILBOX_H_
