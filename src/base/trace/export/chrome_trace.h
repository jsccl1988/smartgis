// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Chrome Trace JSON export for SpanRecorder (mogu-aligned).

#ifndef BASE_TRACE_EXPORT_CHROME_TRACE_H_
#define BASE_TRACE_EXPORT_CHROME_TRACE_H_

#include <algorithm>
#include <string>

#include "base/trace/detail/json_append.h"
#include "base/trace/recorder/span_recorder.h"

namespace base {
namespace trace {

inline std::string export_chrome_trace(const SpanRecorder& recorder) {
  const auto instants = recorder.instants_copy();
  const auto counters = recorder.counters_copy();
  std::string out;
  out.reserve(64 +
              (recorder.span_count() + instants.size() + counters.size()) *
                  128);
  out.append("{\"traceEvents\":[");
  bool first = true;
  auto sep = [&] {
    if (!first) {
      out.push_back(',');
    }
    first = false;
  };
  recorder.for_each_span([&](const Span& s) {
    if (!s.in_chrome_timeline) {
      return;
    }
    const auto dur = (std::max)(std::int64_t{0}, s.end_us - s.start_us);
    sep();
    out.append("{\"name\":\"");
    detail::append_str(&out, s.name);
    out.append("\",\"cat\":\"");
    detail::append_str(&out, s.cat);
    out.append("\",\"ph\":\"X\",\"ts\":");
    detail::append_i64(&out, s.start_us);
    out.append(",\"dur\":");
    detail::append_i64(&out, dur);
    out.append(",\"pid\":1,\"tid\":");
    detail::append_i64(&out, s.tid);
    out.append(",\"args\":{\"node\":");
    detail::append_i64(&out, s.correlate_id);
    out.append(",\"mem_delta\":");
    detail::append_i64(&out, s.mem_delta_bytes);
    out.append(",\"on_critical_path\":");
    out.append(s.on_critical_path ? "true" : "false");
    out.append(",\"status\":\"");
    detail::append_str(&out, s.status);
    out.append("\"");
    if (s.shard_index >= 0) {
      out.append(",\"shard_index\":");
      detail::append_i64(&out, s.shard_index);
    }
    out.append("}}");
  });
  for (const auto& s : instants) {
    sep();
    out.append("{\"name\":\"");
    detail::append_str(&out, s.name);
    out.append("\",\"cat\":\"");
    detail::append_str(&out, s.cat);
    out.append("\",\"ph\":\"i\",\"ts\":");
    detail::append_i64(&out, s.start_us);
    out.append(",\"pid\":1,\"tid\":");
    detail::append_i64(&out, s.tid);
    out.append(",\"s\":\"g\"}");
  }
  for (const auto& c : counters) {
    sep();
    out.append("{\"name\":\"");
    detail::append_str(&out, c.name);
    out.append("\",\"cat\":\"trace,counter\",\"ph\":\"C\",\"ts\":");
    detail::append_i64(&out, c.ts_us);
    out.append(",\"pid\":1,\"tid\":0,\"args\":{\"value\":");
    detail::append_i64(&out, c.value);
    out.append("}}");
  }
  out.append("]}");
  return out;
}

}  // namespace trace
}  // namespace base

#endif  // BASE_TRACE_EXPORT_CHROME_TRACE_H_
