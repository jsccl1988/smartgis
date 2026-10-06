// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_APPLY_H_
#define APP_VIEWS_RUNTIME_INTERACT_APPLY_H_

#include <string>

#include "content/browser/capability/host.h"

namespace app {

// Parses + executes Interact.g4 scripts (.il) against a CapabilityHost.
// Composes wire (parse) · io (read) · host (marks) · exec (verbs).
// ANTLR backends are generated into out/{Debug|Release}/gen (not checked in).
// Returns false if the file cannot be read or parse/exec fails.
bool try_apply_interact(content::CapabilityHost& host,
                        const std::wstring& path);

// True when |path| looks like a .il Interact source.
bool is_interact_path(const std::wstring& path);

}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_APPLY_H_
