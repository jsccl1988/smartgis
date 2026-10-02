// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/trace/diag/startup_profile.h"
#include "base/trace/event/process_trace.h"
#include "base/trace/event/trace.h"
#include "base/trace/export/chrome_trace.h"
#include "base/trace/log/frame_log.h"
#include "base/trace/recorder/span_recorder.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

int main() {
  {
    base::trace::Trace tr;
    {
      base::trace::ScopedTracer scoped(tr, "layout", "map2d.layout");
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
    const auto roll = base::trace::rollup_trace_phases(tr.snapshot_events());
    assert(!roll.empty());
  }

  {
    base::trace::set_tracing_enabled(false);
    {
      BASE_TRACE_EVENT("should_skip", "map2d.layout");
    }
    assert(base::trace::process_trace().size() == 0u);
    base::trace::set_tracing_enabled(true);
    {
      BASE_TRACE_EVENT("gpu_present", "map2d.present");
    }
    assert(base::trace::process_trace().size() >= 1u);
    base::trace::set_tracing_enabled(false);
    base::trace::process_trace().clear();
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
    base::trace::Trace tr;
    tr.add_counter("process_used", "memory", 1024);
    const std::string json = tr.dump();
    assert(json.find("\"ph\":\"C\"") != std::string::npos);
    assert(json.find("\"value\":1024") != std::string::npos);
  }

  {
    base::trace::Trace tr;
    const auto t0 = tr.origin();
    tr.add("RenderMap", "gdi.frame", t0, t0 + std::chrono::milliseconds(10));
    tr.add("roads", "gdi.layer", t0 + std::chrono::milliseconds(1),
           t0 + std::chrono::milliseconds(5));
    tr.add("line", "gdi.geom", t0 + std::chrono::milliseconds(1),
           t0 + std::chrono::milliseconds(4));
    tr.add("compose", "gdi.frame", t0 + std::chrono::milliseconds(8),
           t0 + std::chrono::milliseconds(9));
    const auto lines =
        base::trace::format_trace_frame_log_lines(tr.snapshot_events());
    assert(lines.size() == 1u);
    assert(lines[0].text.find("RenderMap=") != std::string::npos);
    assert(lines[0].text.find("L:roads=") != std::string::npos);
    assert(lines[0].text.find("geom:line=") != std::string::npos);
    assert(lines[0].text.find("compose=") != std::string::npos);
  }

  {
    base::trace::set_tracing_enabled(true);
    base::trace::process_trace().clear();
    {
      BASE_TRACE_EVENT("wWinMain", "startup");
      {
        BASE_TRACE_EVENT("Browser.init", "startup");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    }
    assert(base::trace::process_trace().size() >= 2u);
    const char* dump_path = "startup_profile_test.txt";
    _putenv_s("SMT_STARTUP_PROFILE", "1");
    _putenv_s("SMT_STARTUP_PROFILE_DUMP", dump_path);
    assert(base::trace::startup_profile_wanted());
    base::trace::dump_startup_profile(dump_path);
    std::ifstream in(dump_path, std::ios::binary);
    assert(in.good());
    std::string body((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    assert(body.find("wWinMain") != std::string::npos);
    assert(body.find("Browser.init") != std::string::npos);
    assert(body.find("offset_ms") != std::string::npos);
    std::remove(dump_path);
    std::remove("startup_profile_test.json");
    base::trace::set_tracing_enabled(false);
    base::trace::process_trace().clear();
  }

  std::printf("trace_test OK\n");
  return 0;
}
