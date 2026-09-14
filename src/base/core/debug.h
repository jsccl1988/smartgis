// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CORE_DEBUG_H_
#define BASE_CORE_DEBUG_H_

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>

#include "base/core/log.h"
#include "base/core/macros.h"

namespace base {

// Scoped wall-clock timer that prints elapsed microseconds on destruction.
class ScopedTimer {
 public:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;
  using duration = std::chrono::microseconds;

  explicit ScopedTimer(std::string_view name, std::ostream& os = std::cout)
      : name_(name), os_(os), start_(clock::now()) {}

  ~ScopedTimer() {
    auto end = clock::now();
    auto dur = std::chrono::duration_cast<duration>(end - start_);
    os_ << "[" << name_ << "] elapsed: " << dur.count() << " us\n";
  }

  duration elapsed() const {
    return std::chrono::duration_cast<duration>(clock::now() - start_);
  }

  void reset() { start_ = clock::now(); }

 private:
  std::string name_;
  std::ostream& os_;
  time_point start_;
};

inline void console_error(std::string_view message) {
  std::cerr << message;
  std::cerr.flush();
}

}  // namespace base

#ifndef TRACE_LOCATION
#define TRACE_LOCATION(os, clr)                                         \
  (os << clr << "[" << ::base::detail::log_thread_id() << "]["         \
      << __FUNCTION__ << "][" << __LINE__ << "]")
#endif  // TRACE_LOCATION

#ifndef TIMER_SCOPE
#define TIMER_SCOPE(name) ::base::ScopedTimer ANONYMOUS_VAR(__timer__)(name)
#endif  // TIMER_SCOPE

#ifndef DEBUG_ASSERT
#define DEBUG_ASSERT(condition, message)                                  \
  do {                                                                    \
    if (!(condition)) {                                                   \
      ::base::console_error("ASSERTION FAILED: ");                        \
      ::base::console_error(message);                                     \
      ::base::console_error("\n");                                        \
      LOGGING(LOG_FATAL, "ASSERTION FAILED: %s", (message));              \
      std::abort();                                                       \
    }                                                                     \
  } while (0)
#endif  // DEBUG_ASSERT

#ifndef DEBUG_CHECK
#define DEBUG_CHECK(condition, message)                                   \
  do {                                                                    \
    if (!(condition)) {                                                   \
      LOGGING(LOG_ERROR, "CHECK FAILED: %s", (message));                  \
    }                                                                     \
  } while (0)
#endif  // DEBUG_CHECK

#endif  // BASE_CORE_DEBUG_H_
