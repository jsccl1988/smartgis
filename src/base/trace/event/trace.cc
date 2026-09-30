// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/trace/event/trace.h"

#include <sstream>

namespace base {
namespace trace {

Trace::Trace(size_t max_events)
    : origin_(time_point::clock::now()), max_events_(max_events) {}

Trace::~Trace() = default;

Trace::time_point Trace::origin() const {
  std::lock_guard<std::mutex> lock(mu_);
  return origin_;
}

void Trace::add(std::string_view name, time_point b, time_point e) {
  add(name, "ChromeProfiler", b, e);
}

void Trace::add(std::string_view name, std::string_view cat, time_point b,
                time_point e) {
  Event ev{this_thread::id(), name, cat, b, e};
  std::lock_guard<std::mutex> lock(mu_);
  if (max_events_ > 0 && events_.size() >= max_events_) {
    events_.pop_front();
  }
  events_.push_back(std::move(ev));
}

void Trace::add_counter(std::string_view name, std::string_view cat,
                        int64_t value) {
  const time_point now = time_point::clock::now();
  Event ev{this_thread::id(), name, cat, now, now};
  ev.kind = Event::Kind::kCounter;
  ev.counter_value = value;
  std::lock_guard<std::mutex> lock(mu_);
  if (max_events_ > 0 && events_.size() >= max_events_) {
    events_.pop_front();
  }
  events_.push_back(std::move(ev));
}

void Trace::dump(std::ostream& os) const {
  std::lock_guard<std::mutex> lock(mu_);
  os << "{\"traceEvents\":[";
  bool first = true;
  for (const Event& event : events_) {
    append_event_json(os, first, event);
  }
  os << "]}";
}

std::string Trace::dump() const {
  std::ostringstream oss;
  dump(oss);
  return oss.str();
}

void Trace::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  events_.clear();
  origin_ = time_point::clock::now();
}

std::vector<Trace::Event> Trace::snapshot_events() const {
  std::lock_guard<std::mutex> lock(mu_);
  return std::vector<Event>(events_.begin(), events_.end());
}

Trace::Statistics Trace::get_statistics() const {
  std::lock_guard<std::mutex> lock(mu_);
  Statistics stats;
  for (const Event& e : events_) {
    if (e.kind != Event::Kind::kComplete) {
      continue;
    }
    const size_t dur_us = static_cast<size_t>(duration_us(e.end - e.begin));
    stats.event_count++;
    stats.total_duration_us += dur_us;
    if (stats.min_duration_us == 0 || dur_us < stats.min_duration_us) {
      stats.min_duration_us = dur_us;
    }
    if (dur_us > stats.max_duration_us) {
      stats.max_duration_us = dur_us;
    }
    stats.thread_event_count[e.tid]++;
    stats.name_event_count[e.name]++;
  }
  return stats;
}

size_t Trace::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return events_.size();
}

void Trace::append_event_json(std::ostream& os, bool& first,
                              const Event& event) const {
  using namespace std::chrono;
  if (first) {
    first = false;
  } else {
    os << ',';
  }
  const auto ts = duration_cast<microseconds>(event.begin - origin_).count();
  std::string name_esc;
  std::string cat_esc;
  detail::append_str(&name_esc, event.name);
  detail::append_str(&cat_esc,
                     event.cat.empty() ? "ChromeProfiler" : event.cat);
  if (event.kind == Event::Kind::kCounter) {
    os << "{\"cat\":\"" << cat_esc << "\",\"name\":\"" << name_esc
       << "\",\"ph\":\"C\",\"pid\":1,\"tid\":" << event.tid << ",\"ts\":" << ts
       << ",\"args\":{\"value\":" << event.counter_value << "}}";
    return;
  }
  const auto dur =
      duration_cast<microseconds>(event.end - event.begin).count();
  os << "{\"cat\":\"" << cat_esc << "\",\"name\":\"" << name_esc
     << "\",\"ph\":\"X\",\"pid\":1,\"tid\":" << event.tid << ",\"ts\":" << ts
     << ",\"dur\":" << dur << '}';
}

}  // namespace trace
}  // namespace base
