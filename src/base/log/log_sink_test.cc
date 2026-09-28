// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/log/log_sink.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

int main() {
  base::LogSink sink;
  sink.set_capacity(3);
  sink.clear();

  {
    base::LogEntry a;
    a.level = base::LogLevel::kInfo;
    a.message = "a";
    sink.append(std::move(a));
    base::LogEntry b;
    b.level = base::LogLevel::kInfo;
    b.message = "b";
    sink.append(std::move(b));
    base::LogEntry c;
    c.level = base::LogLevel::kInfo;
    c.message = "c";
    sink.append(std::move(c));
    base::LogEntry d;
    d.level = base::LogLevel::kInfo;
    d.message = "d";
    sink.append(std::move(d));
    assert(sink.size() == 3u);
    const auto tail = sink.snapshot_tail(10);
    assert(tail.size() == 3u);
    assert(tail[0].message == "b");
    assert(tail[1].message == "c");
    assert(tail[2].message == "d");
  }

  {
    sink.clear();
    sink.set_min_level(base::LogLevel::kWarning);
    base::LogEntry info;
    info.level = base::LogLevel::kInfo;
    info.message = "info";
    sink.append(std::move(info));
    base::LogEntry warn;
    warn.level = base::LogLevel::kWarning;
    warn.message = "warn";
    sink.append(std::move(warn));
    assert(sink.size() == 1u);
    assert(sink.snapshot_tail(1)[0].message == "warn");
  }

  {
    sink.clear();
    sink.set_min_level(base::LogLevel::kTrace);
    int seen = 0;
    const auto id = sink.subscribe([&](const base::LogEntry& e) {
      ++seen;
      assert(e.message == "sub");
    });
    base::LogEntry e;
    e.level = base::LogLevel::kDebug;
    e.message = "sub";
    sink.append(std::move(e));
    assert(seen == 1);
    sink.unsubscribe(id);
    sink.append(base::LogEntry{.level = base::LogLevel::kDebug, .message = "x"});
    assert(seen == 1);
  }

  {
    base::LogLevel level = base::LogLevel::kInfo;
    assert(base::parse_log_level("DEBUG", &level));
    assert(level == base::LogLevel::kDebug);
    assert(std::string(base::log_level_name(base::LogLevel::kError)) ==
           "ERROR");
  }

  std::printf("log_sink_test OK\n");
  return 0;
}
