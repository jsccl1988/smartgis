// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// span_recorder — preallocated slots + overflow (mogu-aligned; no mogu queue).

#ifndef BASE_TRACE_RECORDER_SPAN_RECORDER_H_
#define BASE_TRACE_RECORDER_SPAN_RECORDER_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <utility>
#include <vector>

#include "base/trace/recorder/span.h"
#include "base/threading/thread.h"

namespace base {
namespace trace {

class SpanRecorder {
 public:
  static constexpr std::size_t kOverflowBit =
      std::size_t{1} << (sizeof(std::size_t) * 8 - 1);
  static constexpr std::size_t kInvalidSpan = ~std::size_t{0};

  void reserve(std::size_t n) {
    if (spans_.size() < n) {
      spans_.resize(n);
    }
    const std::size_t overflow_n = n < 16 ? 16 : n;
    if (overflow_slots_.size() < overflow_n) {
      overflow_slots_.resize(overflow_n);
    }
    next_span_.store(0, std::memory_order_relaxed);
    next_overflow_.store(0, std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(aux_mu_);
      instants_.clear();
      counters_.clear();
      if (instants_.capacity() < 32) {
        instants_.reserve(32);
      }
      if (counters_.capacity() < 32) {
        counters_.reserve(32);
      }
    }
  }

  std::int64_t now_us() const {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::microseconds>(
               clock::now().time_since_epoch())
        .count();
  }

  std::size_t begin_span(const char* name, const char* cat,
                         std::int64_t correlate_id, int tid = 0,
                         bool in_chrome_timeline = true) {
    if (tid == 0) {
      tid = this_thread::id();
    }
    const std::size_t i = next_span_.fetch_add(1, std::memory_order_relaxed);
    if (i < spans_.size()) {
      init_span(&spans_[i], name, cat, correlate_id, tid, in_chrome_timeline);
      return i;
    }
    const std::size_t o =
        next_overflow_.fetch_add(1, std::memory_order_relaxed);
    if (o < overflow_slots_.size()) {
      init_span(&overflow_slots_[o], name, cat, correlate_id, tid,
                in_chrome_timeline);
      return o | kOverflowBit;
    }
    return kInvalidSpan;
  }

  void end_span(std::size_t idx, std::int64_t mem_delta = 0) {
    Span* e = mutable_span(idx);
    if (!e) {
      return;
    }
    e->end_us = now_us();
    e->mem_delta_bytes = mem_delta;
  }

  void set_span_status(std::size_t idx, const char* status) {
    Span* e = mutable_span(idx);
    if (!e || !status) {
      return;
    }
    std::snprintf(e->status, sizeof(e->status), "%s", status);
  }

  void add_instant(const char* name, const char* cat) {
    Span e;
    e.name = name ? name : "";
    e.cat = cat ? cat : "";
    e.start_us = now_us();
    e.end_us = e.start_us;
    e.tid = this_thread::id();
    std::lock_guard<std::mutex> lock(aux_mu_);
    instants_.push_back(e);
  }

  void set_counter(const char* name, std::int64_t value) {
    CounterSample c{name ? name : "", now_us(), value};
    std::lock_guard<std::mutex> lock(aux_mu_);
    counters_.push_back(c);
  }

  std::size_t span_count() const {
    return primary_span_count() + overflow_span_count();
  }

  std::vector<Span> spans_copy() const {
    std::vector<Span> out;
    out.reserve(span_count());
    for_each_span([&](const Span& s) { out.push_back(s); });
    return out;
  }

  const std::vector<Span>& instants() const {
    std::lock_guard<std::mutex> lock(aux_mu_);
    return instants_;
  }

  const std::vector<CounterSample>& counters() const {
    std::lock_guard<std::mutex> lock(aux_mu_);
    return counters_;
  }

  // Copies for export without holding lock across JSON build.
  std::vector<Span> instants_copy() const {
    std::lock_guard<std::mutex> lock(aux_mu_);
    return instants_;
  }
  std::vector<CounterSample> counters_copy() const {
    std::lock_guard<std::mutex> lock(aux_mu_);
    return counters_;
  }

  void clear() {
    next_span_.store(0, std::memory_order_relaxed);
    next_overflow_.store(0, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(aux_mu_);
    instants_.clear();
    counters_.clear();
  }

  template <class Fn>
  void for_each_span(Fn&& fn) const {
    const std::size_t n = primary_span_count();
    for (std::size_t i = 0; i < n; ++i) {
      fn(spans_[i]);
    }
    const std::size_t o = overflow_span_count();
    for (std::size_t i = 0; i < o; ++i) {
      fn(overflow_slots_[i]);
    }
  }

 private:
  static void init_span(Span* e, const char* name, const char* cat,
                        std::int64_t correlate_id, int tid,
                        bool in_chrome_timeline) {
    e->name = name ? name : "";
    e->cat = cat ? cat : "";
    e->correlate_id = correlate_id;
    e->start_us = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();
    e->end_us = e->start_us;
    e->mem_delta_bytes = 0;
    e->on_critical_path = false;
    e->in_chrome_timeline = in_chrome_timeline;
    e->tid = tid;
    e->shard_index = -1;
    std::snprintf(e->status, sizeof(e->status), "ok");
  }

  std::size_t primary_span_count() const {
    const std::size_t n = next_span_.load(std::memory_order_relaxed);
    return n < spans_.size() ? n : spans_.size();
  }

  std::size_t overflow_span_count() const {
    const std::size_t n = next_overflow_.load(std::memory_order_relaxed);
    return n < overflow_slots_.size() ? n : overflow_slots_.size();
  }

  Span* mutable_span(std::size_t idx) {
    if (idx == kInvalidSpan) {
      return nullptr;
    }
    if (idx & kOverflowBit) {
      const std::size_t o = idx & ~kOverflowBit;
      return o < overflow_slots_.size() ? &overflow_slots_[o] : nullptr;
    }
    return idx < spans_.size() ? &spans_[idx] : nullptr;
  }

  const Span* span_at(std::size_t idx) const {
    return const_cast<SpanRecorder*>(this)->mutable_span(idx);
  }

  std::vector<Span> spans_;
  std::vector<Span> overflow_slots_;
  std::atomic<std::size_t> next_span_{0};
  std::atomic<std::size_t> next_overflow_{0};
  mutable std::mutex aux_mu_;
  mutable std::vector<Span> instants_;
  mutable std::vector<CounterSample> counters_;
};

}  // namespace trace
}  // namespace base

#endif  // BASE_TRACE_RECORDER_SPAN_RECORDER_H_
