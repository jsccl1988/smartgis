// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_MAP_REDUCE_REDUCER_H
#define BASE_EXECUTION_MAP_REDUCE_REDUCER_H

#include <cstddef>
#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/concurrency/partition.h"
#include "base/execution/map_reduce/channel.h"
#include "base/synchronization/latch.h"

namespace base {
namespace execution {
namespace detail {

// Collects reduce outputs into a private buffer (no shared locks).
template <typename OutPair>
class PrivateOutIter {
 public:
  explicit PrivateOutIter(std::vector<OutPair>* out) : out_(out) {}

  PrivateOutIter& operator*() { return *this; }
  PrivateOutIter& operator++() { return *this; }
  PrivateOutIter& operator++(int) { return *this; }

  PrivateOutIter& operator=(const OutPair& v) {
    out_->push_back(v);
    return *this;
  }
  PrivateOutIter& operator=(OutPair&& v) {
    out_->push_back(std::move(v));
    return *this;
  }

 private:
  std::vector<OutPair>* out_;
};

}  // namespace detail

// One worker owns shard_id exclusively: pop from channel, accumulate, reduce
// on EOF, write private out buffer.
template <typename Reducer, typename Key, typename Value>
void reducer_pipeline_helper(
    detail::ShardPipeline<Key, Value>* pipeline, size_t shard_id,
    std::vector<std::pair<Key, typename Reducer::output_type>>* out_buffer,
    base::latch* done_latch) {
  using out_pair = std::pair<Key, typename Reducer::output_type>;
  using kv_type = typename detail::ShardPipeline<Key, Value>::kv_type;
  using value_iter = typename std::vector<Value>::iterator;

  base::scoped_partition_write shard_guard(pipeline->exclusivity(), shard_id);

  auto reducer = std::make_unique<Reducer>();
  reducer->start(static_cast<unsigned int>(shard_id));

  std::unordered_map<Key, std::vector<Value>> groups;
  std::vector<kv_type> bulk(256);

  auto* channel = &pipeline->channel(shard_id);
  for (;;) {
    const size_t n = channel->pop_bulk(bulk.data(), bulk.size());
    if (n > 0) {
      for (size_t i = 0; i < n; ++i) {
        groups[std::move(bulk[i].first)].push_back(
            std::move(bulk[i].second));
      }
      continue;
    }
    if (pipeline->map_eof()) {
      size_t drained;
      while ((drained = channel->pop_bulk(bulk.data(), bulk.size())) > 0) {
        for (size_t i = 0; i < drained; ++i) {
          groups[std::move(bulk[i].first)].push_back(
              std::move(bulk[i].second));
        }
      }
      break;
    }
    std::this_thread::yield();
  }

  detail::PrivateOutIter<out_pair> out(out_buffer);
  for (auto& entry : groups) {
    value_iter begin = entry.second.begin();
    value_iter end = entry.second.end();
    reducer->template reduce<value_iter, detail::PrivateOutIter<out_pair>>(
        entry.first, &begin, &end, out);
  }
  reducer->flush();
  done_latch->count_down();
}
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_MAP_REDUCE_REDUCER_H
