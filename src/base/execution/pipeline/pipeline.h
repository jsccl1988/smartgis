// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_PIPELINE_PIPELINE_H
#define BASE_EXECUTION_PIPELINE_PIPELINE_H

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "base/concurrency/queue.h"
#include "base/core/log.h"
#include "base/core/macros.h"
#include "base/util/path.h"
#include "base/synchronization/align.h"
#include "base/trace/span_recorder.h"

namespace base {
namespace execution {
// Multi-Producer, Multi-Consumer pipeline,
// Stage0(Producer)->Stage1...->StageN(Consumer)
// Optional base::trace::SpanRecorder* profiles each Context × stage call
// (nullptr = off, default). Does not touch common::observe.
//
// Optional context hooks: acquire/release replace new/delete for Context*.
// On CONSUMED / COMPLETE / FAILED the Stage releases the context — handlers
// must not delete &ctx.
namespace detail {
enum class Status {
  FAILED = -1,
  SUCCESS = 0,
  COMPLETE = 1,
  // Sink stage finished this context; Stage releases it (do not enqueue).
  CONSUMED = 2,
};

template <typename Context>
struct ContextHooks {
  std::function<Context*()> acquire;
  std::function<void(Context*)> release;
};

template <typename Context>
class Stage {
 public:
  using Handler = std::function<Status(Context&)>;
  struct Descriptor {
    size_t worker_num{0};
    Handler handler{nullptr};
  };

  Stage(size_t worker_num, Handler handler,
        std::shared_ptr<Stage<Context>> upstream, size_t stage_index = 0,
        base::trace::SpanRecorder* recorder = nullptr,
        const char* stage_name = "stage",
        std::shared_ptr<ContextHooks<Context>> hooks = nullptr)
      : _upstream(std::move(upstream)),
        _handler(std::move(handler)),
        _worker_num(worker_num),
        _recorder(recorder),
        _stage_index(stage_index),
        _stage_name(stage_name ? stage_name : "stage"),
        _hooks(std::move(hooks)) {}

  void set_span_recorder(base::trace::SpanRecorder* recorder) {
    _recorder = recorder;
  }
  void set_stage_name(const char* name) {
    _stage_name = name ? name : "stage";
  }
  void set_context_hooks(std::shared_ptr<ContextHooks<Context>> hooks) {
    _hooks = std::move(hooks);
  }

  inline bool is_finish() {
    return _close && _finish_buffer.size_approx() == 0 &&
           (!_upstream || _upstream->finish_count() == finish_count());
  }
  inline int finish_count(void) {
    return static_cast<int>(_finish_count.load(std::memory_order_relaxed));
  }
  inline bool fetch(Context*& ctx) { return _finish_buffer.pop(ctx); }

  Context* acquire_context() {
    if (_hooks && _hooks->acquire) {
      return _hooks->acquire();
    }
    return new Context();
  }

  void release_context(Context* ctx) {
    if (!ctx) {
      return;
    }
    if (_hooks && _hooks->release) {
      _hooks->release(ctx);
      return;
    }
    delete ctx;
  }

  inline bool forward(Context*& ctx, int worker_index) {
    auto run_handler = [&]() -> Status { return _handler(*ctx); };

    std::size_t span_idx = base::trace::SpanRecorder::kInvalidSpan;
    if (_recorder) {
      const int tid = static_cast<int>(_stage_index * 1000u +
                                       static_cast<unsigned>(worker_index));
      span_idx =
          _recorder->begin_span(_stage_name, "pipeline", -1, tid, true);
    }
    const Status st = run_handler();
    if (st == Status::COMPLETE) {
      if (_recorder && span_idx != base::trace::SpanRecorder::kInvalidSpan) {
        _recorder->set_span_status(span_idx, "done");
        _recorder->end_span(span_idx);
      }
      return false;
    }
    if (st == Status::CONSUMED) {
      if (_recorder && span_idx != base::trace::SpanRecorder::kInvalidSpan) {
        _recorder->end_span(span_idx);
      }
      release_context(ctx);
      ctx = nullptr;
      _finish_count.fetch_add(1, std::memory_order_relaxed);
      return true;
    }
    if (st != Status::SUCCESS) {
      if (_recorder && span_idx != base::trace::SpanRecorder::kInvalidSpan) {
        _recorder->set_span_status(span_idx, "fail");
        _recorder->end_span(span_idx);
      }
      return false;
    }
    if (_recorder && span_idx != base::trace::SpanRecorder::kInvalidSpan) {
      _recorder->end_span(span_idx);
    }
    _finish_buffer.push(ctx);
    _finish_count.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  void run() {
    if (_workers) {
      return;
    }

    _workers = std::make_unique<std::unique_ptr<std::thread>[]>(
        (unsigned int)(_worker_num));
    for (int i = 0; i < static_cast<int>(_worker_num); i++) {
      _workers[i] = std::make_unique<std::thread>([this, i] {
        for (;;) {
          Context* ctx = nullptr;
          if (_upstream) {
            if (UNLIKELY(_upstream->is_finish())) {
              _close = true;
              break;
            }

            if (UNLIKELY(!_upstream->fetch(ctx))) {
              continue;
            }
          } else {
            ctx = acquire_context();
          }

          if (!forward(ctx, i)) {
            // COMPLETE / FAILED before enqueue: producer or mid-stage owns
            // the pointer until here.
            release_context(ctx);
            _close = true;
            break;
          }
        }
      });
    }
  }

  void wait() {
    if (_workers) {
      for (int i = 0; i < static_cast<int>(_worker_num); i++) {
        if (_workers[i] && _workers[i]->joinable()) {
          _workers[i]->join();
        }
      }
    }
  }

 private:
  std::shared_ptr<Stage<Context>> _upstream{nullptr};
  Handler _handler{nullptr};
  std::unique_ptr<std::unique_ptr<std::thread>[]> _workers;
  size_t _worker_num{0};

  base::trace::SpanRecorder* _recorder{nullptr};
  size_t _stage_index{0};
  const char* _stage_name{"stage"};
  std::shared_ptr<ContextHooks<Context>> _hooks;

  base::BlockingQueue<Context*> _finish_buffer;
  alignas(base::hardware_destructive_interference_size)
      std::atomic<bool> _close{false};
  alignas(base::hardware_destructive_interference_size)
      std::atomic<size_t> _finish_count{0};
};
}  // namespace detail

template <typename Context>
class Pipeline {
 public:
  using StageDescriptor = typename detail::Stage<Context>::Descriptor;
  using Status = detail::Status;
  using ContextHooks = detail::ContextHooks<Context>;

  Pipeline(const std::vector<StageDescriptor>& stage_descriptors) {
    if (stage_descriptors.size() < 3) {
      return;
    }

    _stages.resize(stage_descriptors.size());
    _stages[0] = std::make_shared<detail::Stage<Context>>(
        stage_descriptors[0].worker_num, stage_descriptors[0].handler, nullptr,
        /*stage_index=*/0, /*recorder=*/nullptr, "stage", _hooks);

    for (size_t i = 1; i < stage_descriptors.size(); i++) {
      auto& option = stage_descriptors[i];
      _stages[i] = std::make_shared<detail::Stage<Context>>(
          option.worker_num, option.handler, _stages[i - 1], i,
          /*recorder=*/nullptr, "stage", _hooks);
    }
  }

  // Optional; nullptr = profiling disabled (default).
  void set_span_recorder(base::trace::SpanRecorder* recorder) {
    _recorder = recorder;
    for (size_t i = 0; i < _stages.size(); ++i) {
      if (_stages[i]) {
        _stages[i]->set_span_recorder(recorder);
      }
    }
  }

  [[nodiscard]] base::trace::SpanRecorder* span_recorder() const {
    return _recorder;
  }

  void set_stage_names(std::vector<const char*> names) {
    _stage_names = std::move(names);
    for (size_t i = 0; i < _stages.size(); ++i) {
      const char* n = (i < _stage_names.size() && _stage_names[i])
                          ? _stage_names[i]
                          : "stage";
      if (_stages[i]) {
        _stages[i]->set_stage_name(n);
      }
    }
  }

  // Replace Context new/delete. Call before run(). Default = new/delete.
  void set_context_hooks(std::function<Context*()> acquire,
                         std::function<void(Context*)> release) {
    _hooks = std::make_shared<ContextHooks>();
    _hooks->acquire = std::move(acquire);
    _hooks->release = std::move(release);
    for (auto& stage : _stages) {
      if (stage) {
        stage->set_context_hooks(_hooks);
      }
    }
  }

  void run() {
    for (auto& stage : _stages) {
      stage->run();
    }
  }

  void wait() {
    for (auto& stage : _stages) {
      stage->wait();
    }
  }

 private:
  std::vector<std::shared_ptr<detail::Stage<Context>>> _stages;
  base::trace::SpanRecorder* _recorder{nullptr};
  std::vector<const char*> _stage_names;
  std::shared_ptr<ContextHooks> _hooks;
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_PIPELINE_PIPELINE_H
