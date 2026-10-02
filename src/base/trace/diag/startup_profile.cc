// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/trace/diag/startup_profile.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>

#include "base/core/log.h"
#include "base/trace/event/process_trace.h"

namespace base {
namespace trace {
namespace {

struct StartupSpan {
  std::string name;
  double offset_ms = 0.0;
  double dur_ms = 0.0;
  Trace::time_point begin{};
};

bool env_flag_on(const char* name) {
  const char* env = std::getenv(name);
  return env && env[0] == '1' && env[1] == '\0';
}

std::atomic<bool>& dumped_flag() {
  static std::atomic<bool> dumped{false};
  return dumped;
}

// Debug default dump path: <exe_dir>/log/startup_profile.txt
const char* default_dump_path() {
#if defined(NDEBUG)
  return nullptr;
#else
  static char path[MAX_PATH + 32] = {};
  if (path[0]) {
    return path;
  }
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return nullptr;
  }
  char* slash = std::strrchr(module, '\\');
  if (!slash) {
    slash = std::strrchr(module, '/');
  }
  if (!slash) {
    return nullptr;
  }
  *slash = '\0';
  std::snprintf(path, sizeof(path), "%s\\log", module);
  _mkdir(path);
  std::snprintf(path, sizeof(path), "%s\\log\\startup_profile.txt", module);
  return path;
#endif
}

std::vector<StartupSpan> collect_startup_spans() {
  const auto events = process_trace().snapshot_events();
  const auto origin = process_trace().origin();
  std::vector<StartupSpan> spans;
  spans.reserve(events.size());
  for (const auto& e : events) {
    if (e.cat != "startup" || e.kind != Trace::Event::Kind::kComplete) {
      continue;
    }
    if (e.end < e.begin) {
      continue;
    }
    StartupSpan s;
    s.name = e.name;
    s.begin = e.begin;
    s.offset_ms = std::chrono::duration<double, std::milli>(e.begin - origin)
                      .count();
    s.dur_ms =
        std::chrono::duration<double, std::milli>(e.end - e.begin).count();
    spans.push_back(std::move(s));
  }
  std::sort(spans.begin(), spans.end(),
            [](const StartupSpan& a, const StartupSpan& b) {
              if (a.begin != b.begin) {
                return a.begin < b.begin;
              }
              return a.dur_ms > b.dur_ms;
            });
  return spans;
}

// Prefer outer wWinMain/BrowserMain; else first→last span wall coverage.
double compute_wall_ms(const std::vector<StartupSpan>& spans) {
  double total_ms = 0.0;
  for (const auto& s : spans) {
    if ((s.name == "wWinMain" || s.name == "BrowserMain") &&
        s.dur_ms > total_ms) {
      total_ms = s.dur_ms;
    }
  }
  if (total_ms <= 0.0 && !spans.empty()) {
    const auto& first = spans.front();
    const auto& last = spans.back();
    total_ms = (last.offset_ms + last.dur_ms) - first.offset_ms;
  }
  return total_ms;
}

void write_table(FILE* out, const std::vector<StartupSpan>& spans) {
  const double total_ms = compute_wall_ms(spans);
  std::fprintf(out,
               "[startup-profile] phases=%zu wall_ms≈%.1f "
               "(cat=startup; nested spans listed)\n",
               spans.size(), total_ms);
  std::fprintf(out,
               "[startup-profile] %10s %10s  %s\n", "offset_ms", "dur_ms",
               "name");
  for (const auto& s : spans) {
    std::fprintf(out, "[startup-profile] %10.1f %10.1f  %s\n", s.offset_ms,
                 s.dur_ms, s.name.c_str());
  }
}

std::string sibling_chrome_json_path(const char* out_path) {
  std::string json_path(out_path);
  if (json_path.size() > 4 &&
      (json_path.ends_with(".txt") || json_path.ends_with(".TXT"))) {
    json_path.replace(json_path.size() - 4, 4, ".json");
  } else {
    json_path += ".chrome.json";
  }
  return json_path;
}

void write_text_file(const char* out_path,
                     const std::vector<StartupSpan>& spans) {
  if (!out_path || !out_path[0]) {
    return;
  }
  std::ofstream text(out_path, std::ios::binary);
  if (!text) {
    return;
  }
  char line[512];
  const double total_ms = compute_wall_ms(spans);
  std::snprintf(line, sizeof(line),
                "[startup-profile] phases=%zu wall_ms≈%.1f\n", spans.size(),
                total_ms);
  text << line;
  text << "[startup-profile]   offset_ms     dur_ms  name\n";
  for (const auto& s : spans) {
    std::snprintf(line, sizeof(line),
                  "[startup-profile] %10.1f %10.1f  %s\n", s.offset_ms,
                  s.dur_ms, s.name.c_str());
    text << line;
  }
}

void write_chrome_json(const char* out_path) {
  if (!out_path || !out_path[0]) {
    return;
  }
  const std::string json_path = sibling_chrome_json_path(out_path);
  std::ofstream out(json_path, std::ios::binary);
  if (out) {
    out << process_trace().dump();
  }
  std::fprintf(stderr, "[startup-profile] text=%s chrome=%s\n", out_path,
               json_path.c_str());
  std::fflush(stderr);
}

// Build partial dump path from canonical dump path + tag.
std::string make_partial_path(const char* tag) {
  const char* env = std::getenv("SMT_STARTUP_PROFILE_DUMP");
  std::string base;
  if (env && env[0]) {
    base = env;
  } else if (const char* def = default_dump_path()) {
    base = def;
  } else {
    return {};
  }
  const char* safe = (tag && tag[0]) ? tag : "mid";
  // Sanitize tag to [A-Za-z0-9._-] for filesystem safety.
  std::string clean;
  for (const char* p = safe; *p; ++p) {
    const char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
      clean.push_back(c);
    } else {
      clean.push_back('_');
    }
  }
  if (clean.empty()) {
    clean = "mid";
  }
  if (base.size() > 4 &&
      (base.ends_with(".txt") || base.ends_with(".TXT"))) {
    return base.substr(0, base.size() - 4) + ".partial-" + clean + ".txt";
  }
  return base + ".partial-" + clean + ".txt";
}

void dump_startup_profile_impl(const char* path, bool claim_final) {
  const char* out_path = path;
  if (!out_path || !out_path[0]) {
    out_path = default_dump_path();
  }
  const auto spans = collect_startup_spans();
  if (spans.empty()) {
    std::fprintf(stderr,
                 "[startup-profile] no cat=startup events "
                 "(enable always-on diagnostics or SMT_STARTUP_PROFILE=1)\n");
    LOGGING(LOG_INFO, "startup-profile: no cat=startup events");
    return;
  }

  write_table(stderr, spans);
  std::fflush(stderr);
  for (const auto& s : spans) {
    LOGGING(LOG_INFO, "startup-profile: +%.1fms dur=%.1fms %s", s.offset_ms,
            s.dur_ms, s.name.c_str());
  }

  write_text_file(out_path, spans);
  write_chrome_json(out_path);

  if (claim_final) {
    dumped_flag().store(true, std::memory_order_relaxed);
  }
}

}  // namespace

void maybe_init_startup_profile_from_env() {
  if (env_flag_on("SMT_STARTUP_PROFILE")) {
    if (!tracing_enabled()) {
      set_tracing_enabled(true);
    }
  }
}

bool startup_profile_wanted() {
  if (env_flag_on("SMT_STARTUP_PROFILE")) {
    return true;
  }
  if (const char* dump = std::getenv("SMT_STARTUP_PROFILE_DUMP")) {
    if (dump[0]) {
      return true;
    }
  }
#if !defined(NDEBUG)
  return true;
#else
  return false;
#endif
}

void dump_startup_profile(const char* path) {
  dump_startup_profile_impl(path, /*claim_final=*/false);
}

void dump_startup_profile_partial(const char* tag) {
  if (!startup_profile_wanted()) {
    return;
  }
  const std::string path = make_partial_path(tag);
  if (path.empty()) {
    // No file target — still print stderr table for hang diagnosis.
    dump_startup_profile_impl(nullptr, /*claim_final=*/false);
    return;
  }
  dump_startup_profile_impl(path.c_str(), /*claim_final=*/false);
}

void maybe_dump_startup_profile() {
  if (!startup_profile_wanted()) {
    return;
  }
  bool expected = false;
  if (!dumped_flag().compare_exchange_strong(expected, true,
                                             std::memory_order_relaxed)) {
    return;
  }
  const char* path = std::getenv("SMT_STARTUP_PROFILE_DUMP");
  dump_startup_profile_impl(path && path[0] ? path : nullptr,
                            /*claim_final=*/true);
}

}  // namespace trace
}  // namespace base
