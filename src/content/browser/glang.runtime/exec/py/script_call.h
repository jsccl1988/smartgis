// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// One live script call. The CPython module marshals arguments into this
// shape; eval owns the verb lookup. Not a plugin contribution.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_SESSION_SCRIPT_CALL_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_SESSION_SCRIPT_CALL_H_

#include <string>
#include <utility>
#include <vector>

namespace content {
namespace code {

// Values stay strings. The views handler parses ints and "x,y" points.
// Named key "as_" is the Python spelling of "as".
struct ScriptCall {
  std::string name;
  std::vector<std::string> positional;
  std::vector<std::pair<std::string, std::string>> named;
};

// Installed for the duration of a live script. Not owned by the interpreter.
class ScriptHandler {
 public:
  virtual ~ScriptHandler() = default;
  // False raises RuntimeError in the caller. |error| is the exception text.
  // |result| receives a producing verb's slot text when the call succeeds.
  virtual bool call(const ScriptCall& call,
                    std::string* error,
                    std::string* result) = 0;
};

void set_script_handler(ScriptHandler* handler);
void clear_script_handler();
ScriptHandler* script_handler();

}  // namespace code
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_SESSION_SCRIPT_CALL_H_
