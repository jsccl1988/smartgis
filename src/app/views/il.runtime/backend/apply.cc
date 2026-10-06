// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Public entry: load .il, parse, lower, execute against CapabilityHost.

#include "app/views/il.runtime/backend/apply.h"

#include <cstdio>
#include <string>

#include "app/views/il.runtime/backend/exec.h"
#include "app/views/il.runtime/frontend/ast.h"
#include "app/views/il.runtime/frontend/parse.h"
#include "app/views/il.runtime/backend/eval_host.h"
#include "app/views/il.runtime/backend/io.h"
#include "app/views/il.runtime/backend/lower.h"
#include "app/views/util/charset.h"

namespace app {

bool is_execution_path(const std::wstring& path) {
  if (path.size() < 3) {
    return false;
  }
  const size_t n = path.size();
  // .il
  return path[n - 3] == L'.' &&
         (path[n - 2] == L'i' || path[n - 2] == L'I') &&
         (path[n - 1] == L'l' || path[n - 1] == L'L');
}

bool try_apply_execution(content::CapabilityHost& host,
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
  const detail::Code program = detail::lower_script(ast, &vars);
  if (!detail::eval_code(host, program)) {
    std::fprintf(stderr, "interact-dsl: step failed script=%s\n",
                 program.name.c_str());
    std::fflush(stderr);
    detail::host_mark(host, "dsl-step-fail");
    return false;
  }
  detail::host_mark(host, "dsl-done");
  return true;
}

}  // namespace app
