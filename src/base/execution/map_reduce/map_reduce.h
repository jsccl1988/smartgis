// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_MAP_REDUCE_MAP_REDUCE_H
#define BASE_EXECUTION_MAP_REDUCE_MAP_REDUCE_H

// Pipelined in-process MapReduce (A2): mappers emit into per-shard
// NonblockingQueue channels; one reduce worker per shard; private out-buffers;
// single-thread assemble into OutputIter. See
// docs/superpowers/specs/2026-09-10-base-map-reduce-pipeline-design.md
//
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3446.pdf

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "base/execution/execution_executor.h"
#include "base/execution/map_reduce/channel.h"
#include "base/execution/map_reduce/io.h"
#include "base/execution/map_reduce/mapper.h"
#include "base/execution/map_reduce/reducer.h"
#include "base/synchronization/latch.h"

namespace base {
namespace execution {

template <typename Input, typename Key, typename Value>
class Mapper {
 public:
  using input_type = Input;
  using key_type = Key;
  using value_type = Value;

  virtual void start() {}
  virtual void flush() {}

  template <typename OutputIter>
  void map(const Input& input, OutputIter output){};
};

template <typename Key, typename Value, typename Output>
class Reducer {
 public:
  using key_type = Key;
  using value_type = Value;
  using output_type = Output;

  virtual void start(unsigned int shard_id) {}
  virtual void flush() {}

  template <typename InputIter, typename OutputIter>
  void reduce(const Key& key, InputIter* start, InputIter* end,
              OutputIter output){};
};

template <typename Input, typename J>
class IdentitySplitter {
 public:
  template <typename Fn>
  void apply(const Input& front, Fn&& fn) {
    fn(front);
  }
};

template <typename Type>
class DefaultShard {
 public:
  DefaultShard() {}
  size_t operator()(const Type& val, size_t num_shards) {
    return _hash_fn(val) % num_shards;
  }

 private:
  std::hash<Type> _hash_fn;
};

template <typename Key, typename Value>
class IdentityReducer {
 public:
  using key_type = Key;
  using value_type = Value;
  using output_type = Value;

  void start(unsigned int /*shard_id*/) {}
  void flush() {}

  template <typename InputIter, typename OutputIter>
  void reduce(const Key& key, InputIter* start, InputIter* end,
              OutputIter out) {
    for (InputIter* iter = start; (*iter) != (*end); ++(*iter)) {
      out = std::pair<Key, Value>(key, *(*iter));
    }
  }
};

template <typename InputIter, typename OutputIter, typename Mapper,
          typename Reducer,
          typename Combiner = IdentityReducer<typename Reducer::key_type,
                                              typename Reducer::value_type>,
          typename ShardFn = DefaultShard<typename Mapper::key_type>,
          typename InputSplitter = IdentitySplitter<
              typename InputIter::value_type, typename Mapper::input_type>,
          typename Executor = base::execution::NThreadPoolExecutor>
class MapReduce {
 public:
  struct Option {
    size_t mappers{1};
    size_t reduce_shards{1};
    // Kept for API compatibility; pipeline binds one worker per shard.
    size_t reducers{1};

    size_t emit_batch{128};
    size_t channel_high_watermark{0};  // 0 = unlimited

    Executor* executor{nullptr};
    OutputIter output;
    // Caller must size executor for at least (mappers + reduce_shards) worker
    // threads: reduce workers are scheduled first and spin until map EOF, so
    // a smaller pool deadlocks (map tasks never run).
  };

  explicit MapReduce(const Option& option) : _option(option) {}

  void run(InputIter iter, InputIter end) {
    using key_type = typename Mapper::key_type;
    using value_type = typename Mapper::value_type;
    using out_pair = std::pair<key_type, typename Reducer::output_type>;
    using pipeline_t = detail::ShardPipeline<key_type, value_type>;

    const size_t mappers = _option.mappers == 0 ? 1 : _option.mappers;
    const size_t shards =
        _option.reduce_shards == 0 ? 1 : _option.reduce_shards;

    pipeline_t pipeline(shards, mappers, _option.emit_batch,
                        _option.channel_high_watermark);

    std::vector<std::vector<out_pair>> out_buffers(shards);
    base::latch reducer_latch(shards);
    base::latch mapper_latch(mappers);

    // Start reduce workers first so channels drain while maps run.
    for (size_t s = 0; s < shards; ++s) {
      _option.executor->execute(
          &reducer_pipeline_helper<Reducer, key_type, value_type>, &pipeline, s,
          &out_buffers[s], &reducer_latch);
    }

    ShardFn shard_fn;
    for (size_t i = 0; i < mappers; ++i) {
      // Competing ConcurrentQueue pops partition work; plain iterators share
      // the same range (caller should prefer queue-backed input for M>1).
      _option.executor->execute(
          &mapper_pipeline_helper<InputIter, Mapper, InputSplitter, ShardFn,
                                  key_type, value_type>,
          iter, end, &pipeline, shard_fn, &mapper_latch);
    }

    mapper_latch.wait();
    reducer_latch.wait();

    // Single-threaded assemble — OutputIter may wrap a plain map; do not use
    // ConcurrentMap on the hot path.
    for (auto& buf : out_buffers) {
      for (auto& kv : buf) {
        _option.output = std::move(kv);
      }
    }
  }

 private:
  Option _option;
};

}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_MAP_REDUCE_MAP_REDUCE_H
