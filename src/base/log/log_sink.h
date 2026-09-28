// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_LOG_LOG_SINK_H_
#define BASE_LOG_LOG_SINK_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "base/core/export.h"

namespace base {

// Severity for process-wide log sink (aligned with LOGGING color macros).
enum class LogLevel {
  kFatal,
  kError,
  kWarning,
  kNotice,
  kInfo,
  kDebug,
  kTrace,
};

// One captured log line stored in the ring buffer.
struct LogEntry {
  LogLevel level = LogLevel::kInfo;
  std::string timestamp;
  int tid = 0;
  std::string file;
  int line = 0;
  std::string func;
  std::string message;
};

// Thread-safe ring buffer + subscriber fan-out for debug console / agent.
class BASE_EXPORT LogSink {
 public:
  using Subscriber = std::function<void(const LogEntry&)>;

  void set_capacity(std::size_t n);
  std::size_t capacity() const;

  void set_min_level(LogLevel level);
  LogLevel min_level() const;

  void append(LogEntry entry);
  void clear();
  std::vector<LogEntry> snapshot_tail(std::size_t n) const;
  std::size_t size() const;

  // Callback may run on the logging thread while the sink mutex is held.
  // Do not call back into LogSink from the subscriber.
  std::uint64_t subscribe(Subscriber cb);
  void unsubscribe(std::uint64_t id);

 private:
  mutable std::mutex mu_;
  std::size_t capacity_ = 4096;
  LogLevel min_level_ = LogLevel::kTrace;
  std::deque<LogEntry> entries_;
  std::uint64_t next_id_ = 1;
  std::vector<std::pair<std::uint64_t, Subscriber>> subs_;
};

BASE_EXPORT LogSink& log_sink();

BASE_EXPORT const char* log_level_name(LogLevel level);
BASE_EXPORT bool parse_log_level(const std::string& name, LogLevel* out);

// Formats a message into the process sink (used by LOGGING macro).
BASE_EXPORT void log_write(const char* level_token,
                           const char* file,
                           int line,
                           const char* func,
                           const char* fmt,
                           ...);

}  // namespace base

#endif  // BASE_LOG_LOG_SINK_H_
