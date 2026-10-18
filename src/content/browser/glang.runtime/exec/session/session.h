// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_SESSION_SESSION_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_SESSION_SESSION_H_

#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

#include <string>

namespace content {

// One session call. Set exactly one source: suite_id, path_utf8, or code.
// suite_id resolves harness/<family>/<id>/<id>.py and runs the compiler.
// path_utf8 runs that .py the same way. code is one embedded-CPython eval
// (no interact slot, no mark clear; mark leaf is the UI mark file).
struct ScriptCall {
  const char* suite_id = nullptr;
  const char* path_utf8 = nullptr;
  const char* code = nullptr;
  const wchar_t* mark_leaf = nullptr;
  bool clear_marks = true;
};

// rc is the gate code (0 = success). text is the RPC line for a path or an
// eval; a suite run leaves text empty and reports failure only in rc.
struct ScriptStatus {
  int rc = 1;
  std::string text;
  bool ok() const { return rc == 0; }
};

ScriptStatus run_script(content::ShellBridge& browser, const ScriptCall& call);

}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_SESSION_SESSION_H_
