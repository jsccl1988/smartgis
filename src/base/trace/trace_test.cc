// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/trace/chrome_trace.h"
#include "base/trace/process_trace.h"
#include "base/trace/span_recorder.h"
#include "base/trace/trace.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <thread>

int main() {
  {
    base::Trace tr;
    {
      base::ScopedTracer scoped(tr, "layout", "map2d.layout");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    tr.add("upload", "map2d.upload", tr.origin(),
           tr.origin() + std::chrono::microseconds(500));
    assert(tr.size() == 2u);
    const std::string json = tr.dump();
    assert(json.find("\"traceEvents\":[") != std::string::npos);
    assert(json.find("\"name\":\"layout\"") != std::string::npos);
    assert(json.find("\"cat\":\"map2d.layout\"") != std::string::npos);
    assert(json.find("\"ph\":\"X\"") != std::string::npos);
    const auto roll = base::rollup_trace_phases(tr.snapshot_events());
    assert(!roll.empty());
  }

  {
    base::set_tracing_enabled(false);
    {
      BASE_TRACE_EVENT("should_skip", "map2d.layout");
    }
    assert(base::process_trace().size() == 0u);
    base::set_tracing_enabled(true);
    {
      BASE_TRACE_EVENT("gpu_present", "map2d.present");
    }
    assert(base::process_trace().size() >= 1u);
    base::set_tracing_enabled(false);
    base::process_trace().clear();
  }

  {
    base::trace::SpanRecorder rec;
    rec.reserve(4);
    const auto i = rec.begin_span("parse", "pipeline", -1, 1001, true);
    assert(i != base::trace::SpanRecorder::kInvalidSpan);
    rec.end_span(i);
    const auto spans = rec.spans_copy();
    assert(spans.size() == 1u);
    assert(std::string(spans[0].name) == "parse");
    assert(std::string(spans[0].cat) == "pipeline");
    assert(spans[0].tid == 1001);
    assert(spans[0].end_us >= spans[0].start_us);

    rec.reserve(1);
    assert(rec.begin_span("a", "t", 0, 0, true) !=
           base::trace::SpanRecorder::kInvalidSpan);
    const auto o = rec.begin_span("b", "t", 1, 0, true);
    assert(o != base::trace::SpanRecorder::kInvalidSpan);
    rec.end_span(o);
    assert(rec.span_count() == 2u);

    rec.add_instant("mark", "map2d");
    rec.set_counter("draw_items", 42);
    const std::string chrome = base::trace::export_chrome_trace(rec);
    assert(chrome.find("\"traceEvents\":[") != std::string::npos);
    assert(chrome.find("\"ph\":\"i\"") != std::string::npos);
    assert(chrome.find("\"ph\":\"C\"") != std::string::npos);
  }

  {
    base::Trace tr;
    tr.add_counter("process_used", "memory", 1024);
    const std::string json = tr.dump();
    assert(json.find("\"ph\":\"C\"") != std::string::npos);
    assert(json.find("\"value\":1024") != std::string::npos);
  }

  std::printf("trace_test OK\n");
  return 0;
}
