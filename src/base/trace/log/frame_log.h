// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Text log lines for process_trace consumers (MFC RenderTrace dock, tests).
// Groups complete spans under each frame anchor name (default "RenderMap").

#ifndef BASE_TRACE_LOG_FRAME_LOG_H_
#define BASE_TRACE_LOG_FRAME_LOG_H_

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/trace/event/trace.h"

namespace base {
namespace trace {

struct TraceFrameLogLine {
  Trace::time_point frame_end{};
  std::string text;
};

namespace detail {

inline void append_sorted_ms(
    std::string* text,
    const std::unordered_map<std::string, int64_t>& us_by_name,
    const char* prefix) {
  if (!text || us_by_name.empty()) {
    return;
  }
  text->append(prefix);
  std::vector<std::pair<std::string, int64_t>> sorted(us_by_name.begin(),
                                                     us_by_name.end());
  std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  bool first = true;
  for (const auto& kv : sorted) {
    char part[128];
    std::snprintf(part, sizeof(part), "%s%s=%.2f", first ? "" : " ",
                  kv.first.c_str(),
                  static_cast<double>(kv.second) / 1000.0);
    text->append(part);
    first = false;
  }
}

}  // namespace detail

// Build one log line per complete span whose name == frame_anchor and whose
// cat starts with cat_prefix. Nested complete spans (begin/end inside the
// frame) are rolled into frame-level names, "L:…", and "geom:…".
inline std::vector<TraceFrameLogLine> format_trace_frame_log_lines(
    const std::vector<Trace::Event>& events,
    std::string_view cat_prefix = "gdi.",
    std::string_view frame_anchor = "RenderMap") {
  std::vector<const Trace::Event*> complete;
  complete.reserve(events.size());
  for (const auto& e : events) {
    if (e.kind != Trace::Event::Kind::kComplete) {
      continue;
    }
    if (!cat_prefix.empty() && e.cat.find(cat_prefix) != 0) {
      continue;
    }
    complete.push_back(&e);
  }
  std::sort(complete.begin(), complete.end(),
            [](const Trace::Event* a, const Trace::Event* b) {
              return a->begin < b->begin;
            });

  std::vector<TraceFrameLogLine> out;
  for (const Trace::Event* frame : complete) {
    if (frame->name != frame_anchor) {
      continue;
    }
    const auto frame_us = std::chrono::duration_cast<Trace::duration>(
                              frame->end - frame->begin)
                              .count();
    std::unordered_map<std::string, int64_t> layer_us;
    std::unordered_map<std::string, int64_t> geom_us;
    std::unordered_map<std::string, int64_t> other_us;
    for (const Trace::Event* e : complete) {
      if (e == frame) {
        continue;
      }
      if (e->begin < frame->begin || e->end > frame->end) {
        continue;
      }
      const auto us =
          std::chrono::duration_cast<Trace::duration>(e->end - e->begin)
              .count();
      if (e->cat == "gdi.layer" || e->cat.starts_with("gdi.layer.")) {
        layer_us[e->name] += us;
      } else if (e->cat == "gdi.geom" || e->cat.starts_with("gdi.geom.")) {
        geom_us[e->name] += us;
      } else if (e->name != frame_anchor) {
        other_us[e->name] += us;
      }
    }

    char head[96];
    std::snprintf(head, sizeof(head), "RenderMap=%.2fms",
                  static_cast<double>(frame_us) / 1000.0);
    std::string text = head;
    detail::append_sorted_ms(&text, other_us, " |");
    detail::append_sorted_ms(&text, layer_us, " | L:");
    detail::append_sorted_ms(&text, geom_us, " | geom:");
    out.push_back(TraceFrameLogLine{frame->end, std::move(text)});
  }
  return out;
}

}  // namespace trace
}  // namespace base

#endif  // BASE_TRACE_LOG_FRAME_LOG_H_
