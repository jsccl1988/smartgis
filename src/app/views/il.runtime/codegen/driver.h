// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_DRIVER_H_
#define IL_RUNTIME_CODEGEN_DRIVER_H_

#include <string>

namespace content {
struct CapabilityHost;
}

namespace app {

// Compiler driver: pick a frontend from the path suffix. No Browser, no
// path search, no Host fill. Runtime packs never include this header.
enum class ScriptKind {
  kIl,
  kPython,
  kUnknown,
};

ScriptKind script_kind(const std::wstring& path);

// Run |path| on an already-linked Host. .il 鈫?apply / lower / exec.
// .py is recognized and rejected until a Python host is linked; it must
// not parse Interact.g4.
bool apply_script(content::CapabilityHost& host, const std::wstring& path);

}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_DRIVER_H_
