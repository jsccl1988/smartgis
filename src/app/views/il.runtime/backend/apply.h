// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_EXECUTION_APPLY_H_
#define IL_RUNTIME_EXECUTION_APPLY_H_

#include <string>

#include "content/browser/capability/host.h"

namespace app {

// IL pipeline driver: io read → front parse → eval interpret onto ir:: calls.
// ANTLR backends are generated into out/{Debug|Release}/gen (not checked in).
// Returns false if the file cannot be read or parse/exec fails.
bool try_apply_execution(content::CapabilityHost& host,
                         const std::wstring& path);

// True when |path| looks like a .il Interact source.
bool is_execution_path(const std::wstring& path);

}  // namespace app

#endif  // IL_RUNTIME_EXECUTION_APPLY_H_
