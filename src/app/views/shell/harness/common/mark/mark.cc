// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/mark/mark.h"

#include "app/views/shell/util/exe_sidecar_path.h"

#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_set>

namespace app {
namespace detail {
namespace {

std::mutex g_mark_mu;
std::unordered_set<std::wstring> g_trunc_done;

}  // namespace

void clear_mark(const wchar_t* leaf) {
  if (!leaf) {
    return;
  }
  wchar_t path[MAX_PATH] = {};
  if (!exe_capture_path(path, MAX_PATH, leaf)) {
    return;
  }
  DeleteFileW(path);
  std::lock_guard<std::mutex> lock(g_mark_mu);
  g_trunc_done.erase(leaf);
}

void write_mark(const wchar_t* leaf, const char* step, bool truncate) {
  if (!leaf || !step) {
    return;
  }
  wchar_t path[MAX_PATH] = {};
  if (!exe_capture_path(path, MAX_PATH, leaf)) {
    return;
  }
  bool use_trunc = false;
  if (truncate) {
    std::lock_guard<std::mutex> lock(g_mark_mu);
    use_trunc = g_trunc_done.insert(leaf).second;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, use_trunc ? L"w" : L"a") != 0 || !f) {
    return;
  }
  std::fprintf(f, "%s\n", step);
  std::fflush(f);
  std::fclose(f);
}

}  // namespace detail
}  // namespace app
