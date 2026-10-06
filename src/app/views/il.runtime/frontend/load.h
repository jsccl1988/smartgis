// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_FRONTEND_LOAD_H_
#define IL_RUNTIME_FRONTEND_LOAD_H_

#include <string>

namespace app {

// Translation-unit lookup (compiler source manager). No CapabilityHost,
// no Browser, no parse. Suite ids resolve to
// testing/tools/harness/<family>/<id>/<id>.il (override: --ui-interact-script).
bool resolve_suite_script(const char* suite_id, std::wstring* out);

bool script_file_exists(const std::wstring& path);

}  // namespace app

#endif  // IL_RUNTIME_FRONTEND_LOAD_H_
