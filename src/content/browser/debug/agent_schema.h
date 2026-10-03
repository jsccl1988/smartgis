// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_SCHEMA_H_
#define CONTENT_BROWSER_DEBUG_AGENT_SCHEMA_H_

#include <string>

namespace content {
namespace detail {

// Machine-readable Agent method catalog (JSON Schema-ish object).
// Used by rpc.methods / :help json and tools/debug consumers.
std::string agent_methods_schema_json();

// Human :help text (multi-line).
std::string agent_help_text();

// Compact command prefix list for Tab completion.
const char* const* agent_console_command_prefixes(size_t* count);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_SCHEMA_H_
