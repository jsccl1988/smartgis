// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_MAP_REDUCE_MAPPER_H
#define BASE_EXECUTION_MAP_REDUCE_MAPPER_H

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/execution/map_reduce/channel.h"
#include "base/synchronization/latch.h"

namespace base {
namespace execution {
namespace detail {

// Routes Mapper::map outputs into ShardPipeline with local batching.
// Shared state so OutputIter-by-value copies stay coherent.
template <typename Key, typename Value, typename ShardFn>
class PipelineEmitIter {
 public:
  using kv_type = typename ShardPipeline<Key, Value>::kv_type;

  PipelineEmitIter(ShardPipeline<Key, Value>* pipeline, ShardFn* shard_fn)
      : state_(std::make_shared<State>(pipeline, shard_fn)) {}

  PipelineEmitIter& operator*() { return *this; }
  PipelineEmitIter& operator++() { return *this; }
  PipelineEmitIter& operator++(int) { return *this; }

  PipelineEmitIter& operator=(const std::pair<Key, Value>& kv) {
    return assign(kv);
  }

  PipelineEmitIter& operator=(std::pair<Key, Value>&& kv) {
    return assign(std::move(kv));
  }

  void flush_all() {
    for (size_t s = 0; s < state_->batches.size(); ++s) {
      flush_shard(s);
    }
  }

 private:
  struct State {
    State(ShardPipeline<Key, Value>* pipeline, ShardFn* shard_fn)
        : pipeline(pipeline), shard_fn(shard_fn) {
      batches.resize(pipeline->shard_count());
    }
    ShardPipeline<Key, Value>* pipeline;
    ShardFn* shard_fn;
    std::vector<std::vector<kv_type>> batches;
  };

  template <typename Kv>
  PipelineEmitIter& assign(Kv&& kv) {
    const size_t shard =
        (*state_->shard_fn)(kv.first, state_->pipeline->shard_count());
    auto& batch = state_->batches[shard];
    batch.push_back(std::forward<Kv>(kv));
    if (batch.size() >= state_->pipeline->emit_batch()) {
      flush_shard(shard);
    }
    return *this;
  }

  void flush_shard(size_t shard) {
    auto& batch = state_->batches[shard];
    if (batch.empty()) {
      return;
    }
    state_->pipeline->emit_bulk(shard, batch.data(), batch.size());
    batch.clear();
  }

  std::shared_ptr<State> state_;
};

}  // namespace detail

template <typename InputIter, typename Mapper, typename InputSplitter,
          typename ShardFn, typename Key, typename Value>
void mapper_pipeline_helper(InputIter iter, InputIter iter_end,
                            detail::ShardPipeline<Key, Value>* pipeline,
                            ShardFn shard_fn, base::latch* done_latch) {
  auto mapper = std::make_unique<Mapper>();
  InputSplitter splitter;
  detail::PipelineEmitIter<Key, Value, ShardFn> emit(pipeline, &shard_fn);

  auto apply_fn = [&](const typename Mapper::input_type& input) {
    mapper->template map<detail::PipelineEmitIter<Key, Value, ShardFn>>(
        input, emit);
  };

  while (iter != iter_end) {
    auto value = *(iter++);
    mapper->start();
    splitter.apply(value, apply_fn);
    mapper->flush();
  }

  emit.flush_all();
  pipeline->notify_mapper_done();
  done_latch->count_down();
}
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_MAP_REDUCE_MAPPER_H
