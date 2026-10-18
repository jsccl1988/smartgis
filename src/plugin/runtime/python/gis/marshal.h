// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Python values in, glang GIS calls out. Analysis / scene / style share this.

#ifndef PLUGIN_RUNTIME_PYTHON_GIS_MARSHAL_H_
#define PLUGIN_RUNTIME_PYTHON_GIS_MARSHAL_H_

#include "plugin/runtime/python/bind/py_compat.h"

#include "content/browser/glang.runtime/exec/extend/exec.h"

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
PyObject* il_flag(const char* op,
                  const content::code::GisArg* args,
                  int count,
                  bool refresh);

PyObject* make_analysis_module();
PyObject* make_scene_module();
PyObject* make_style_module();

#endif

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_GIS_MARSHAL_H_
