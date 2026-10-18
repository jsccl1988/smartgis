// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Process-wide CPython 3.12. Plugin packs and suite scripts share this
// interpreter. init() runs on the chrome thread. Debug builds keep the stub
// (no python312.lib); Release links the embeddable prefix.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_SESSION_CPYTHON_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_SESSION_CPYTHON_H_

#include <string>
#include <string_view>

namespace content {
namespace py {

// Called around Py_InitializeFromConfig. before_initialize may append inittab
// entries. after_initialize runs only when the interpreter is up.
struct InitHooks {
  bool (*before_initialize)() = nullptr;
  void (*after_initialize)() = nullptr;
};

// One interpreter for the process. shutdown() is safe when init() never ran.
class Interpreter {
 public:
  bool init(const InitHooks& hooks = {});
  void shutdown();
  bool is_ready() const;

  // Evaluate |code| in __main__ (stdout plus expression repr). Never throws.
  std::string eval(std::string_view code);

  // Execute |code| as statements in __main__.
  bool exec_source(std::string_view code, std::string* error);

  // Load |path| as a fresh module, call main(), then drop that module.
  bool exec_file(std::string_view path, std::string* error);

 private:
  bool ready_ = false;
};

Interpreter& interpreter();

}  // namespace py
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_SESSION_CPYTHON_H_
