// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TRACE_SPAN_H_
#define BASE_TRACE_SPAN_H_

#include <cstdint>

namespace base {
namespace trace {

// Generic profile span / counter sample (mogu-aligned hot-path POD).
struct Span {
  const char* name = "";
  const char* cat = "";
  std::int64_t correlate_id = -1;
  std::int64_t start_us = 0;
  std::int64_t end_us = 0;
  std::int64_t mem_delta_bytes = 0;
  bool on_critical_path = false;
  bool in_chrome_timeline = true;
  int tid = 0;
  int shard_index = -1;
  char status[8] = "ok";
};

struct CounterSample {
  const char* name = "";
  std::int64_t ts_us = 0;
  std::int64_t value = 0;
};

}  // namespace trace
}  // namespace base

#endif  // BASE_TRACE_SPAN_H_
