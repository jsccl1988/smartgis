// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CORE_LOG_H_
#define BASE_CORE_LOG_H_

#include <cstdio>
#include <ctime>

#include "base/core/build_config.h"
#include "base/core/macros.h"

#if defined(OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef EOF
#define EOF (-1)
#endif

namespace base {
namespace detail {

// Thread id for LOGGING without requiring base/threading (Phase 2).
inline int log_thread_id() {
#if defined(OS_WIN)
  thread_local DWORD tls_id = 0;
  thread_local bool initialized = false;
  if (UNLIKELY(!initialized)) {
    tls_id = GetCurrentThreadId();
    initialized = true;
  }
  return static_cast<int>(tls_id);
#else
  thread_local int tls_id = 0;
  thread_local bool initialized = false;
  if (UNLIKELY(!initialized)) {
    tls_id = static_cast<int>(getpid());
    initialized = true;
  }
  return tls_id;
#endif
}

inline const char* log_timestamp_cached() {
  thread_local time_t last_sec = 0;
  thread_local char buf[32] = {};
  time_t now = time(nullptr);
  if (now != last_sec) {
    last_sec = now;
#if defined(OS_WIN)
    struct tm tm_buf = {};
    localtime_s(&tm_buf, &now);
    size_t n = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
#else
    struct tm tm_buf = {};
    localtime_r(&now, &tm_buf);
    size_t n = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
#endif
    buf[n] = '\0';
  }
  return buf;
}

}  // namespace detail
}  // namespace base

#define CLR_RESET "\033[0m"
#define CLR_BLACK "\033[30m"
#define CLR_RED "\033[31m"
#define CLR_GREEN "\033[32m"
#define CLR_YELLOW "\033[33m"
#define CLR_BLUE "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN "\033[36m"
#define CLR_WHITE "\033[37m"
#define CLR_BOLDBLACK "\033[1m\033[30m"
#define CLR_BOLDRED "\033[1m\033[31m"
#define CLR_BOLDGREEN "\033[1m\033[32m"
#define CLR_BOLDYELLOW "\033[1m\033[33m"
#define CLR_BOLDBLUE "\033[1m\033[34m"
#define CLR_BOLDMAGENTA "\033[1m\033[35m"
#define CLR_BOLDCYAN "\033[1m\033[36m"
#define CLR_BOLDWHITE "\033[1m\033[37m"

#define LOG_DEVICE stderr
#define LOG_FATAL CLR_BOLDRED
#define LOG_ERROR CLR_BOLDRED
#define LOG_WARNING CLR_BOLDYELLOW
#define LOG_NOTICE CLR_BOLDBLUE
#define LOG_INFO CLR_BLUE
#define LOG_DEBUG CLR_BOLDMAGENTA
#define LOG_TRACE CLR_BOLDCYAN

// fflush: when stderr is redirected to a file, libc may fully buffer it.
#define LOGGING(level, ...)                                                   \
  do {                                                                        \
    fprintf(LOG_DEVICE, "%s [%s] %s TID [%d] FUNC:[%s] [%d] [%s] ", level,   \
            #level, ::base::detail::log_timestamp_cached(),                   \
            ::base::detail::log_thread_id(), __FILE__, __LINE__,              \
            __FUNCTION__);                                                    \
    fprintf(LOG_DEVICE, __VA_ARGS__);                                         \
    fprintf(LOG_DEVICE, "\n");                                                \
    fflush(LOG_DEVICE);                                                       \
  } while (0)

#endif  // BASE_CORE_LOG_H_
