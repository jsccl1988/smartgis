// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_MAP_REDUCE_CHANNEL_H
#define BASE_EXECUTION_MAP_REDUCE_CHANNEL_H

#include <atomic>
#include <cstddef>
#include <thread>
#include <utility>
#include <vector>

#include "base/concurrency/partition.h"
#include "base/concurrency/queue.h"
#include "base/synchronization/align.h"

namespace base {
namespace execution {
namespace detail {

// MPSC shard channels for A2 pipelined MapReduce: many mappers push, one
// reducer per shard pops. Hot path has no ConcurrentMap / mutex map.
template <typename Key, typename Value>
class ShardPipeline {
 public:
  using kv_type = std::pair<Key, Value>;
  using channel_t = base::NonblockingQueue<kv_type>;

  ShardPipeline(size_t shard_count, size_t mapper_count, size_t emit_batch,
                size_t high_watermark)
      : emit_batch_(emit_batch == 0 ? 1 : emit_batch),
        high_watermark_(high_watermark),
        mappers_remaining_(mapper_count),
        exclusivity_(shard_count) {
    channels_.resize(shard_count);
  }

  size_t shard_count() const { return channels_.size(); }
  size_t emit_batch() const { return emit_batch_; }
  base::partition_exclusivity& exclusivity() { return exclusivity_; }

  channel_t& channel(size_t shard_id) { return channels_[shard_id]; }

  bool map_eof() const {
    return map_eof_.load(std::memory_order_acquire);
  }

  void notify_mapper_done() {
    const size_t left =
        mappers_remaining_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (left == 0) {
      map_eof_.store(true, std::memory_order_release);
    }
  }

  // Push one KV with optional backpressure when high_watermark_ > 0.
  void emit(size_t shard_id, kv_type kv) {
    wait_if_over_watermark(shard_id);
    channels_[shard_id].push(std::move(kv));
  }

  template <typename It>
  void emit_bulk(size_t shard_id, It first, size_t count) {
    if (count == 0) {
      return;
    }
    wait_if_over_watermark(shard_id);
    channels_[shard_id].push_bulk(first, count);
  }

 private:
  void wait_if_over_watermark(size_t shard_id) {
    if (high_watermark_ == 0) {
      return;
    }
    while (channels_[shard_id].size_approx() >= high_watermark_ &&
           !map_eof()) {
      std::this_thread::yield();
    }
  }

  size_t emit_batch_;
  size_t high_watermark_;
  alignas(base::hardware_destructive_interference_size)
      std::atomic<size_t> mappers_remaining_;
  alignas(base::hardware_destructive_interference_size)
      std::atomic<bool> map_eof_{false};
  std::vector<channel_t> channels_;
  base::partition_exclusivity exclusivity_;
};

}  // namespace detail
}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_MAP_REDUCE_CHANNEL_H
