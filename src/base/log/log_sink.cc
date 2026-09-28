// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/log/log_sink.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <utility>

#include "base/core/log.h"

namespace base {
namespace {

int level_rank(LogLevel level) {
  return static_cast<int>(level);
}

std::string to_upper_ascii(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return s;
}

}  // namespace

void LogSink::set_capacity(std::size_t n) {
  std::lock_guard<std::mutex> lock(mu_);
  capacity_ = n == 0 ? 1 : n;
  while (entries_.size() > capacity_) {
    entries_.pop_front();
  }
}

std::size_t LogSink::capacity() const {
  std::lock_guard<std::mutex> lock(mu_);
  return capacity_;
}

void LogSink::set_min_level(LogLevel level) {
  std::lock_guard<std::mutex> lock(mu_);
  min_level_ = level;
}

LogLevel LogSink::min_level() const {
  std::lock_guard<std::mutex> lock(mu_);
  return min_level_;
}

void LogSink::append(LogEntry entry) {
  std::vector<Subscriber> to_notify;
  {
    std::lock_guard<std::mutex> lock(mu_);
    // Higher severity has lower enum rank (kFatal=0). Keep entries at or
    // above min_level (rank <= min_level rank when min is Trace=keep all).
    if (level_rank(entry.level) > level_rank(min_level_)) {
      return;
    }
    entries_.push_back(entry);
    while (entries_.size() > capacity_) {
      entries_.pop_front();
    }
    to_notify.reserve(subs_.size());
    for (const auto& pair : subs_) {
      to_notify.push_back(pair.second);
    }
  }
  for (const Subscriber& cb : to_notify) {
    if (cb) {
      cb(entry);
    }
  }
}

void LogSink::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  entries_.clear();
}

std::vector<LogEntry> LogSink::snapshot_tail(std::size_t n) const {
  std::lock_guard<std::mutex> lock(mu_);
  if (n == 0 || entries_.empty()) {
    return {};
  }
  const std::size_t start =
      entries_.size() > n ? entries_.size() - n : 0;
  return std::vector<LogEntry>(entries_.begin() + static_cast<std::ptrdiff_t>(start),
                               entries_.end());
}

std::size_t LogSink::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return entries_.size();
}

std::uint64_t LogSink::subscribe(Subscriber cb) {
  std::lock_guard<std::mutex> lock(mu_);
  const std::uint64_t id = next_id_++;
  subs_.emplace_back(id, std::move(cb));
  return id;
}

void LogSink::unsubscribe(std::uint64_t id) {
  std::lock_guard<std::mutex> lock(mu_);
  subs_.erase(std::remove_if(subs_.begin(), subs_.end(),
                             [id](const auto& p) { return p.first == id; }),
              subs_.end());
}

LogSink& log_sink() {
  static LogSink sink;
  return sink;
}

const char* log_level_name(LogLevel level) {
  switch (level) {
    case LogLevel::kFatal:
      return "FATAL";
    case LogLevel::kError:
      return "ERROR";
    case LogLevel::kWarning:
      return "WARNING";
    case LogLevel::kNotice:
      return "NOTICE";
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kDebug:
      return "DEBUG";
    case LogLevel::kTrace:
      return "TRACE";
  }
  return "INFO";
}

bool parse_log_level(const std::string& name, LogLevel* out) {
  if (!out) {
    return false;
  }
  const std::string u = to_upper_ascii(name);
  if (u == "FATAL" || u == "LOG_FATAL") {
    *out = LogLevel::kFatal;
    return true;
  }
  if (u == "ERROR" || u == "LOG_ERROR") {
    *out = LogLevel::kError;
    return true;
  }
  if (u == "WARNING" || u == "WARN" || u == "LOG_WARNING") {
    *out = LogLevel::kWarning;
    return true;
  }
  if (u == "NOTICE" || u == "LOG_NOTICE") {
    *out = LogLevel::kNotice;
    return true;
  }
  if (u == "INFO" || u == "LOG_INFO") {
    *out = LogLevel::kInfo;
    return true;
  }
  if (u == "DEBUG" || u == "LOG_DEBUG") {
    *out = LogLevel::kDebug;
    return true;
  }
  if (u == "TRACE" || u == "LOG_TRACE") {
    *out = LogLevel::kTrace;
    return true;
  }
  return false;
}

void log_write(const char* level_token,
               const char* file,
               int line,
               const char* func,
               const char* fmt,
               ...) {
  LogLevel level = LogLevel::kInfo;
  if (level_token) {
    parse_log_level(level_token, &level);
  }
  char buf[2048];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt ? fmt : "", ap);
  va_end(ap);

  LogEntry entry;
  entry.level = level;
  entry.timestamp = detail::log_timestamp_cached();
  entry.tid = detail::log_thread_id();
  entry.file = file ? file : "";
  entry.line = line;
  entry.func = func ? func : "";
  entry.message = buf;
  log_sink().append(std::move(entry));
}

}  // namespace base
