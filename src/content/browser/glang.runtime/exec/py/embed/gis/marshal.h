// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Python values in, glang GIS calls out. Analysis / scene / style share this.

#ifndef PLUGIN_RUNTIME_PYTHON_GIS_MARSHAL_H_
#define PLUGIN_RUNTIME_PYTHON_GIS_MARSHAL_H_

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"

#include "content/browser/glang.runtime/exec/vm/exec.h"

#include <sstream>
#include <string>

namespace plugin {
namespace detail {

#if defined(HAS_PYTHON)

content::code::GisArg text_arg(std::string text);
bool call_il(const char* name,
             const content::code::GisArg* args,
             int count,
             std::string* result,
             std::string* error);
void append_json_scalar(std::ostringstream& out, PyObject* value);
PyObject* loads_json(const std::string& text);
PyObject* bridge_required(const char* what);
// Zero-arg capability call. On failure sets a Python error and returns false.
bool il_out(const char* op, std::string* out);
// Same call. Empty text and the JSON null token become None.
PyObject* il_json_or_none(const char* op);
// One text argument. |ready| is the console-bridge slot for |missing|.
PyObject* il_flag_text(const char* missing,
                       bool ready,
                       const char* op,
                       const char* text,
                       bool refresh);
PyObject* il_flag(const char* op,
                  const content::code::GisArg* args,
                  int count,
                  bool refresh);

#endif

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_GIS_MARSHAL_H_
