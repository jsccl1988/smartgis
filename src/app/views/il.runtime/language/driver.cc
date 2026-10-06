// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/language/backend.h"

#include "app/views/il.runtime/backend/apply.h"

#include <cstdio>
#include <cwchar>
#include <string>

namespace app {
namespace {

bool ends_with_ci(const std::wstring& path, const wchar_t* suffix) {
  const size_t n = std::wcslen(suffix);
  if (path.size() < n) {
    return false;
  }
  const std::wstring tail = path.substr(path.size() - n);
  return _wcsicmp(tail.c_str(), suffix) == 0;
}

}  // namespace

ScriptKind script_kind(const std::wstring& path) {
  if (ends_with_ci(path, L".py")) {
    return ScriptKind::kPython;
  }
  if (is_execution_path(path)) {
    return ScriptKind::kIl;
  }
  return ScriptKind::kUnknown;
}

bool apply_script(content::CapabilityHost& host, const std::wstring& path) {
  switch (script_kind(path)) {
    case ScriptKind::kIl:
      return try_apply_execution(host, path);
    case ScriptKind::kPython:
      std::fprintf(stderr, "run_script: .py backend is not linked\n");
      std::fflush(stderr);
      return false;
    case ScriptKind::kUnknown:
      break;
  }
  return false;
}

}  // namespace app
