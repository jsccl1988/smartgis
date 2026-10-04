// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/process/switches.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace base {
namespace {

std::mutex g_mu;
std::unordered_map<std::string, std::string> g_values;

const std::unordered_set<std::string> kSgKeys = {
    "debug",
    "debug-allow",
    "debug-port",
    "python",
    "console-bench-json",
    "sdbd-base",
    "sdbd-rpc",
    "mogu-root",
};

const std::unordered_set<std::string> kSkipForward = {
    "type",
    "parent-pid",
    "pipe",
    "session",
};

std::string normalize_key(std::string_view raw) {
  std::string k;
  k.reserve(raw.size());
  for (unsigned char c : raw) {
    if (c == '_') {
      k.push_back('-');
    } else {
      k.push_back(static_cast<char>(std::tolower(c)));
    }
  }
  if (k.rfind("smt-", 0) == 0) {
    k.erase(0, 4);
  } else if (k.rfind("sg-", 0) == 0) {
    k.erase(0, 3);
  }
  return k;
}

std::string wide_to_utf8(const wchar_t* s) {
  if (!s || !s[0]) {
    return {};
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr,
                                    nullptr);
  if (n <= 1) {
    return {};
  }
  std::string out(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, s, -1, out.data(), n, nullptr, nullptr);
  return out;
}

std::wstring utf8_to_wide(std::string_view s) {
  if (s.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                    static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) {
    return {};
  }
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                      out.data(), n);
  return out;
}

void sync_leftover_env(const std::string& kebab, const std::string& value) {
  if (kSgKeys.contains(kebab) || kebab == "cursor-api-key" ||
      kebab == "smartgis-root" || kebab == "userprofile") {
    return;
  }
  std::string env = "SMT_";
  env.reserve(kebab.size() + 4);
  for (char c : kebab) {
    env.push_back(c == '-' ? '_' : static_cast<char>(std::toupper(
                                       static_cast<unsigned char>(c))));
  }
  _putenv_s(env.c_str(), value.c_str());
}

void set_locked(std::string key, std::string value) {
  auto it = g_values.find(key);
  if (it != g_values.end()) {
    it->second = std::move(value);
    sync_leftover_env(key, it->second);
    return;
  }
  auto [ins, _] = g_values.emplace(std::move(key), std::move(value));
  sync_leftover_env(ins->first, ins->second);
}

void ingest_token(std::string_view token, std::string_view next, bool* used_next) {
  if (used_next) {
    *used_next = false;
  }
  if (token.size() < 3 || token[0] != '-' || token[1] != '-') {
    return;
  }
  const std::string_view body = token.substr(2);
  std::string key;
  std::string val;
  const auto eq = body.find('=');
  if (eq != std::string_view::npos) {
    key = normalize_key(body.substr(0, eq));
    val.assign(body.substr(eq + 1));
  } else {
    key = normalize_key(body);
    if (!next.empty() && (next.size() < 2 || next[0] != '-' || next[1] != '-')) {
      val.assign(next);
      if (used_next) {
        *used_next = true;
      }
    } else {
      val = "1";
    }
  }
  if (key.empty()) {
    return;
  }
  set_locked(std::move(key), std::move(val));
}

}  // namespace

void init_switches_from_argv(int argc, const wchar_t* const* argv) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!argv) {
    return;
  }
  for (int i = 1; i < argc; ++i) {
    const std::string tok = wide_to_utf8(argv[i]);
    const std::string next =
        (i + 1 < argc) ? wide_to_utf8(argv[i + 1]) : std::string{};
    bool used_next = false;
    ingest_token(tok, next, &used_next);
    if (used_next) {
      ++i;
    }
  }
}

void init_switches_from_argv(int argc, const char* const* argv) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!argv) {
    return;
  }
  for (int i = 1; i < argc; ++i) {
    const char* tok = argv[i] ? argv[i] : "";
    const char* next = (i + 1 < argc && argv[i + 1]) ? argv[i + 1] : "";
    bool used_next = false;
    ingest_token(tok, next, &used_next);
    if (used_next) {
      ++i;
    }
  }
}

void set_switch(std::string_view key, std::string_view value) {
  std::lock_guard<std::mutex> lock(g_mu);
  set_locked(normalize_key(key), std::string(value));
}

void clear_switch(std::string_view key) {
  std::lock_guard<std::mutex> lock(g_mu);
  const std::string k = normalize_key(key);
  g_values.erase(k);
  sync_leftover_env(k, "");
}

void clear_switches_for_test() {
  std::lock_guard<std::mutex> lock(g_mu);
  for (const auto& kv : g_values) {
    sync_leftover_env(kv.first, "");
  }
  g_values.clear();
}

const char* switch_cstr(std::string_view key) {
  std::lock_guard<std::mutex> lock(g_mu);
  const auto it = g_values.find(normalize_key(key));
  if (it == g_values.end() || it->second.empty()) {
    return nullptr;
  }
  return it->second.c_str();
}

bool switch_is_one(std::string_view key) {
  const char* v = switch_cstr(key);
  return v && v[0] == '1' && v[1] == '\0';
}

bool switch_is_zero(std::string_view key) {
  const char* v = switch_cstr(key);
  return v && v[0] == '0' && v[1] == '\0';
}

int switch_int(std::string_view key, int fallback) {
  const char* v = switch_cstr(key);
  if (!v || !v[0]) {
    return fallback;
  }
  return std::atoi(v);
}

void append_switches_to_command_line(std::wstring* cmd) {
  if (!cmd) {
    return;
  }
  std::vector<std::pair<std::string, std::string>> snap;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    snap.reserve(g_values.size());
    for (const auto& kv : g_values) {
      if (kSkipForward.contains(kv.first)) {
        continue;
      }
      snap.emplace_back(kv.first, kv.second);
    }
  }
  for (const auto& kv : snap) {
    *cmd += L" --";
    *cmd += utf8_to_wide(kv.first);
    *cmd += L"=";
    *cmd += utf8_to_wide(kv.second);
  }
}

}  // namespace base
