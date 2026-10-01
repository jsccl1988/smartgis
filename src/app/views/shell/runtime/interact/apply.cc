// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Public entry: load .il, parse, execute against CapabilityHost.

#include "app/views/shell/runtime/interact/apply.h"

#include <windows.h>

#include <cstdio>
#include <string>

#include "app/views/shell/runtime/interact/ast.h"
#include "app/views/shell/runtime/interact/exec.h"
#include "app/views/shell/runtime/interact/host_util.h"
#include "app/views/shell/runtime/interact/parse.h"

namespace app {
namespace {

bool read_file_utf8(const std::wstring& path, std::string* out) {
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) {
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  const long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz < 0 || sz > 4 * 1024 * 1024) {
    std::fclose(f);
    return false;
  }
  out->resize(static_cast<size_t>(sz));
  const size_t n = std::fread(out->data(), 1, out->size(), f);
  std::fclose(f);
  out->resize(n);
  return n > 0;
}

}  // namespace

bool is_interact_path(const std::wstring& path) {
  if (path.size() < 3) {
    return false;
  }
  const size_t n = path.size();
  // .il
  return path[n - 3] == L'.' &&
         (path[n - 2] == L'i' || path[n - 2] == L'I') &&
         (path[n - 1] == L'l' || path[n - 1] == L'L');
}

bool try_apply_interact(content::CapabilityHost& host,
                        const std::wstring& path) {
  std::string src;
  if (!read_file_utf8(path, &src)) {
    std::fwprintf(stderr, L"interact-dsl: read failed %ls\n", path.c_str());
    return false;
  }
  detail::ScriptAst ast;
  std::string err;
  if (!detail::parse_interact_source(src, &ast, &err)) {
    std::fprintf(stderr, "interact-dsl: parse error: %s\n", err.c_str());
    detail::host_mark(host, "dsl-parse-fail");
    return false;
  }
  detail::host_mark(host, "dsl-load-ok");
  detail::VarMap vars;
  {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
      const std::wstring dir_w = path.substr(0, slash);
      char dir_a[MAX_PATH * 3] = {};
      if (WideCharToMultiByte(CP_UTF8, 0, dir_w.c_str(), -1, dir_a,
                              static_cast<int>(sizeof(dir_a)), nullptr,
                              nullptr) > 0) {
        vars["script_dir"] = dir_a;
      }
    }
  }
  for (const detail::Stmt& s : ast.stmts) {
    if (!detail::exec_stmt(host, s, &vars)) {
      if (s.kind == detail::Stmt::Kind::kCall) {
        std::fprintf(stderr, "interact-dsl: step failed call=%s\n",
                     s.call.name.c_str());
      } else {
        std::fprintf(stderr, "interact-dsl: step failed block\n");
      }
      std::fflush(stderr);
      detail::host_mark(host, "dsl-step-fail");
      return false;
    }
  }
  detail::host_mark(host, "dsl-done");
  return true;
}

}  // namespace app
