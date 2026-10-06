// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Public entry: load .il, parse, execute against CapabilityHost.

#include "app/views/runtime/interact/apply.h"

#include <windows.h>

#include <cstdio>
#include <string>

#include "app/views/runtime/interact/wire/ast.h"
#include "app/views/runtime/interact/exec/exec.h"
#include "app/views/runtime/interact/host/host.h"
#include "app/views/runtime/interact/io/io.h"
#include "app/views/runtime/interact/wire/parse.h"

namespace app {

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
  if (!detail::read_utf8_file(path, &src, 4 * 1024 * 1024)) {
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
      std::string dir_a;
      if (detail::wide_to_utf8(path.substr(0, slash), &dir_a)) {
        vars["script_dir"] = dir_a;
      }
    }
  }
  for (const detail::Stmt& s : ast.stmts) {
    if (!detail::exec_stmt(host, s, &vars)) {
      if (const detail::CallStmt* call = detail::as_call(s)) {
        std::fprintf(stderr, "interact-dsl: step failed call=%s\n",
                     call->name.c_str());
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
