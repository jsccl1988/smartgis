// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Language extension: lower an extra call surface into IL ops.
// Does not include the ANTLR parser. Sema can call lower_extended_call later.

#ifndef IL_RUNTIME_CODEGEN_EXTEND_LANGUAGE_H_
#define IL_RUNTIME_CODEGEN_EXTEND_LANGUAGE_H_

#include "content/browser/glang.runtime/language/code/module.h"

#include <string>
#include <string_view>

namespace content {
namespace code {

// One positional or named string. The extension does not own the text.
struct NamedArg {
  std::string_view key;
  std::string_view value;
};

// Narrow call, so a plugin header does not include the Python grammar.
struct CallLowering {
  std::string_view name;
  const std::string* positional = nullptr;
  int positional_count = 0;
  const NamedArg* named = nullptr;
  int named_count = 0;
  Loc loc{};
};

// lower_call returns false and leaves `error` empty when the name is not
// this extension's. A non-empty error stops the search.
struct LanguageExtension {
  const char* name = nullptr;
  bool (*lower_call)(const CallLowering& call, Module* extra, std::string* error) =
      nullptr;
};

void register_language_extension(const LanguageExtension& ext);

// First extension that returns true wins. An extension that sets `error`
// stops the walk.
bool lower_extended_call(const CallLowering& call, Module* extra, std::string* error);

}  // namespace code
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_EXTEND_LANGUAGE_H_
