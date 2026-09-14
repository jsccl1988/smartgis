// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_THREADING_THREAD_H_
#define BASE_THREADING_THREAD_H_

#include <chrono>
#include <cstring>
#include <exception>
#include <string>
#include <thread>

#include "base/core/build_config.h"
#include "base/core/macros.h"

#if defined(OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(OS_LINUX)
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#elif defined(OS_MACOSX)
#include <pthread.h>
#endif

namespace base {

// Current-thread helpers (mogu-aligned; Windows uses Win32 thread APIs).
struct this_thread {
  inline static int id() {
#if defined(OS_WIN)
    thread_local DWORD tls_id = 0;
    thread_local bool initialized = false;
    if (UNLIKELY(!initialized)) {
      tls_id = GetCurrentThreadId();
      initialized = true;
    }
    return static_cast<int>(tls_id);
#elif defined(OS_LINUX)
    thread_local int tls_id = 0;
    thread_local bool initialized = false;
    if (UNLIKELY(!initialized)) {
      tls_id = static_cast<int>(syscall(SYS_gettid));
      initialized = true;
    }
    return tls_id;
#elif defined(OS_MACOSX)
    thread_local uint64_t tls_id = 0;
    thread_local bool initialized = false;
    if (UNLIKELY(!initialized)) {
      pthread_threadid_np(nullptr, &tls_id);
      initialized = true;
    }
    return static_cast<int>(tls_id);
#else
    return static_cast<int>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
  }

  static void set_name(const char* name) {
#if defined(OS_WIN)
    if (name) {
      std::wstring wname(name, name + strlen(name));
      SetThreadDescription(GetCurrentThread(), wname.c_str());
    }
#elif defined(OS_LINUX)
    pthread_setname_np(pthread_self(), name);
#elif defined(OS_MACOSX)
    pthread_setname_np(name);
#else
    (void)name;
#endif
  }

  inline static std::string get_name() {
#if defined(OS_WIN)
    PWSTR name = nullptr;
    GetThreadDescription(GetCurrentThread(), &name);
    std::string result;
    if (name) {
      int len = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr,
                                    nullptr);
      if (len > 0) {
        result.resize(static_cast<size_t>(len - 1));
        WideCharToMultiByte(CP_UTF8, 0, name, -1, result.data(), len, nullptr,
                            nullptr);
      }
      LocalFree(name);
    }
    return result;
#elif defined(OS_LINUX) || defined(OS_MACOSX)
    char cached_name[64] = {};
    pthread_getname_np(pthread_self(), cached_name, sizeof(cached_name));
    return std::string(cached_name);
#else
    return std::string();
#endif
  }

  static void yield() { std::this_thread::yield(); }

  template <typename Rep, typename Period>
  static void sleep_for(const std::chrono::duration<Rep, Period>& duration) {
    std::this_thread::sleep_for(duration);
  }
};

// RAII join-on-destroy wrapper around std::thread (mogu-aligned).
class jthread {
 public:
  jthread(std::thread other) : inner_(std::move(other)) {}

  ~jthread() {
    if (inner_.joinable()) {
      try {
        inner_.join();
      } catch (...) {
      }
    }
  }

  jthread(const jthread&) = delete;
  jthread& operator=(const jthread&) = delete;
  jthread(jthread&&) noexcept = default;
  jthread& operator=(jthread&&) noexcept = default;

  inline bool joinable() const { return inner_.joinable(); }

  inline void join() {
    if (inner_.joinable()) {
      inner_.join();
    }
  }

  inline void detach() {
    if (inner_.joinable()) {
      inner_.detach();
    }
  }

  bool try_join() {
    if (!inner_.joinable()) {
      return false;
    }
    try {
      inner_.join();
      return true;
    } catch (...) {
      return false;
    }
  }

 private:
  std::thread inner_;
};

}  // namespace base

#endif  // BASE_THREADING_THREAD_H_
