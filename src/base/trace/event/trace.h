// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// base::trace::Trace — Chrome Trace–style event buffer (mogu-aligned; mutex
// ring). Method bodies live in base.dll (trace.cc) so deque/string mutations
// and snapshots never cross DLL boundaries under MSVC debug iterators.

#ifndef BASE_TRACE_EVENT_TRACE_H_
#define BASE_TRACE_EVENT_TRACE_H_

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/core/export.h"
#include "base/threading/thread.h"
#include "base/trace/detail/json_append.h"

namespace base {
namespace trace {

// Thread-safe Chrome Trace–style event buffer. Producers push completed spans;
// consumers dump JSON, snapshot, or aggregate stats. All mutating / snapshot
// methods are exported from base.dll — do not re-inline them in other modules.
class Trace {
 public:
  using time_point = std::chrono::time_point<std::chrono::steady_clock>;
  using duration = std::chrono::microseconds;

  struct Event {
    int tid = 0;
    time_point begin{};
    time_point end{};
    std::string name;
    std::string cat;
    // kComplete → Horizon ph "X"; kCounter → ph "C" (counter_value).
    enum class Kind : uint8_t { kComplete = 0, kCounter = 1 };
    Kind kind = Kind::kComplete;
    int64_t counter_value = 0;

    Event() = default;
    Event(int t, std::string_view n, std::string_view c, time_point b,
          time_point e)
        : tid(t), begin(b), end(e), name(n), cat(c) {}
  };

  struct Statistics {
    size_t event_count = 0;
    size_t total_duration_us = 0;
    size_t min_duration_us = 0;
    size_t max_duration_us = 0;
    std::unordered_map<int, size_t> thread_event_count;
    std::unordered_map<std::string, size_t> name_event_count;
  };

  BASE_EXPORT explicit Trace(size_t max_events = 65536);
  BASE_EXPORT ~Trace();

  Trace(const Trace&) = delete;
  Trace& operator=(const Trace&) = delete;

  BASE_EXPORT time_point origin() const;

  BASE_EXPORT void add(std::string_view name, time_point b, time_point e);
  BASE_EXPORT void add(std::string_view name, std::string_view cat,
                       time_point b, time_point e);

  // Chrome Trace counter sample (ph "C").
  BASE_EXPORT void add_counter(std::string_view name, std::string_view cat,
                               int64_t value);

  // Chrome Trace Event Format object: {"traceEvents":[...]}
  BASE_EXPORT void dump(std::ostream& os) const;
  BASE_EXPORT std::string dump() const;

  BASE_EXPORT void clear();

  BASE_EXPORT std::vector<Event> snapshot_events() const;

  BASE_EXPORT Statistics get_statistics() const;

  BASE_EXPORT size_t size() const;

 private:
  static int64_t duration_us(std::chrono::steady_clock::duration d) {
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
  }

  void append_event_json(std::ostream& os, bool& first,
                         const Event& event) const;

  mutable std::mutex mu_;
  time_point origin_;
  size_t max_events_ = 65536;
  std::deque<Event> events_;
};

struct TraceSpanUs {
  std::string name;
  int64_t ts_us = 0;
  int64_t dur_us = 0;
  int tid = 0;
};

struct TracePhaseRollup {
  std::string name;
  uint64_t count = 0;
  double avg_us = 0;
  uint64_t p99_us = 0;
  uint64_t min_us = 0;
  uint64_t max_us = 0;
};

inline std::vector<TraceSpanUs> build_trace_spans(
    const std::vector<Trace::Event>& events, const Trace::time_point& origin) {
  std::vector<TraceSpanUs> out;
  out.reserve(events.size());
  for (const auto& e : events) {
    if (e.kind != Trace::Event::Kind::kComplete) {
      continue;
    }
    const auto ts =
        std::chrono::duration_cast<Trace::duration>(e.begin - origin).count();
    const auto dur =
        std::chrono::duration_cast<Trace::duration>(e.end - e.begin).count();
    out.push_back(TraceSpanUs{e.name, ts, dur, e.tid});
  }
  std::sort(out.begin(), out.end(),
            [](const TraceSpanUs& a, const TraceSpanUs& b) {
              return a.ts_us < b.ts_us;
            });
  return out;
}

inline std::vector<TracePhaseRollup> rollup_trace_phases(
    const std::vector<Trace::Event>& events) {
  std::unordered_map<std::string, std::vector<uint64_t>> by_name;
  for (const auto& e : events) {
    const auto dur =
        std::chrono::duration_cast<Trace::duration>(e.end - e.begin).count();
    by_name[e.name].push_back(static_cast<uint64_t>(dur));
  }
  std::vector<TracePhaseRollup> out;
  out.reserve(by_name.size());
  for (auto& kv : by_name) {
    auto& durs = kv.second;
    std::sort(durs.begin(), durs.end());
    const size_t n = durs.size();
    uint64_t sum = 0;
    for (uint64_t v : durs) {
      sum += v;
    }
    size_t p99_idx = 0;
    if (n > 0) {
      p99_idx =
          static_cast<size_t>(std::ceil(0.99 * static_cast<double>(n))) - 1;
      if (p99_idx >= n) {
        p99_idx = n - 1;
      }
    }
    out.push_back(TracePhaseRollup{
        kv.first, n,
        n ? static_cast<double>(sum) / static_cast<double>(n) : 0.0,
        n ? durs[p99_idx] : 0, n ? durs.front() : 0, n ? durs.back() : 0});
  }
  std::sort(out.begin(), out.end(),
            [](const TracePhaseRollup& a, const TracePhaseRollup& b) {
              return a.name < b.name;
            });
  return out;
}

template <typename Tracer>
struct ScopedTracer {
  using time_point = std::chrono::time_point<std::chrono::steady_clock>;

  Tracer& tracer;
  std::string name;
  std::string cat;
  time_point begin;

  ScopedTracer(Tracer& tracer_, std::string_view name_,
               std::string_view cat_ = "ChromeProfiler")
      : tracer(tracer_),
        name(name_),
        cat(cat_),
        begin(time_point::clock::now()) {}

  ~ScopedTracer() { tracer.add(name, cat, begin, time_point::clock::now()); }
};

}  // namespace trace
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_H_
