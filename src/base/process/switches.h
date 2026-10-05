// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_PROCESS_SWITCHES_H_
#define BASE_PROCESS_SWITCHES_H_

#include <string>
#include <string_view>

#include "base/core/export.h"

namespace base {

// Process-wide CLI switches. Keys are kebab-case (--map2d-engine, --trace).
// Init ingests product environment variables (UPPER_SNAKE, optional legacy
// SMT_/SG_ prefixes), then argv (CLI wins). Tests may set().

BASE_EXPORT void init_switches_from_argv(int argc, const wchar_t* const* argv);
BASE_EXPORT void init_switches_from_argv(int argc, const char* const* argv);

BASE_EXPORT void set_switch(std::string_view key, std::string_view value);
BASE_EXPORT void clear_switch(std::string_view key);
BASE_EXPORT void clear_switches_for_test();

// nullptr when unset. Pointer valid until the next set/clear of that key.
BASE_EXPORT const char* switch_cstr(std::string_view key);
BASE_EXPORT bool switch_is_one(std::string_view key);
BASE_EXPORT bool switch_is_zero(std::string_view key);
BASE_EXPORT int switch_int(std::string_view key, int fallback = 0);

// Appends ` --key=value` for every stored switch except child-launch reserved
// keys (type, parent-pid, pipe, session).
BASE_EXPORT void append_switches_to_command_line(std::wstring* cmd);

}  // namespace base

#endif  // BASE_PROCESS_SWITCHES_H_
