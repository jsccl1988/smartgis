// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/gis/marshal.h"

#include "base/core/log.h"
#include "content/browser/glang.runtime/exec/session/plugin_fn.h"
#include "content/browser/glang.runtime/host/bind/gis_console.h"
#include "plugin/runtime/python/bind/gil.h"

#include <string>
#include <string_view>
#include <utility>

namespace plugin {
#if defined(HAS_PYTHON)
namespace detail {

content::code::GisArg text_arg(std::string text) {
  content::code::GisArg arg;
  arg.text = std::move(text);
  arg.kind = content::code::ArgKind::kStr;
  return arg;
}

bool call_il(const char* name,
             const content::code::GisArg* args,
             int count,
             std::string* result,
             std::string* error) {
  // The capability table is the only lookup. console_text then runs the
  // same eval pointer that register_fn copied for this name.
  if (!name || !content::py::find_fn(name)) {
    const std::string text =
        std::string("unknown verb '") + (name ? name : "") + "'";
    if (error) {
      *error = text;
    }
    LOGGING(LOG_ERROR, "plugin gis: %s", text.c_str());
    return false;
  }
  content::code::ConsoleText text;
  {
    ReleaseGil released;
    text = content::code::console_text(name, args, count, false);
  }
  if (!text.ok) {
    if (error) {
      *error = text.error;
    }
    return false;
  }
  if (result) {
    *result = text.text;
  }
  return true;
}

std::string json_escape_path(std::string_view path) {
  std::string out;
  out.reserve(path.size());
  for (char c : path) {
    if (c == '\\') {
      out += '/';
    } else if (c == '"') {
      out += "\\\"";
    } else {
      out += c;
    }
  }
  return out;
}

void append_json_scalar(std::ostringstream& out, PyObject* v) {
  if (v == Py_None) {
    out << "null";
  } else if (PyBool_Check(v)) {
    out << (v == Py_True ? "true" : "false");
  } else if (PyLong_Check(v)) {
    out << PyLong_AsLongLong(v);
  } else if (PyFloat_Check(v)) {
    out << PyFloat_AsDouble(v);
  } else if (PyUnicode_Check(v)) {
    const char* s = PyUnicode_AsUTF8(v);
    out << '"' << json_escape_path(s ? s : "") << '"';
  } else {
    PyObject* as_s = PyObject_Str(v);
    const char* s = as_s ? PyUnicode_AsUTF8(as_s) : "";
    out << '"' << json_escape_path(s ? s : "") << '"';
    Py_XDECREF(as_s);
  }
}

PyObject* bridge_required(const char* what) {
  PyErr_Format(PyExc_RuntimeError, "gis console bridge missing: %s", what);
  return nullptr;
}

PyObject* loads_json(const std::string& text) {
  PyObject* json_mod = PyImport_ImportModule("json");
  if (!json_mod) {
    return nullptr;
  }
  PyObject* loads = PyObject_GetAttrString(json_mod, "loads");
  Py_DECREF(json_mod);
  if (!loads) {
    return nullptr;
  }
  PyObject* py_s =
      PyUnicode_FromStringAndSize(text.data(), static_cast<Py_ssize_t>(text.size()));
  if (!py_s) {
    Py_DECREF(loads);
    return nullptr;
  }
  PyObject* out = PyObject_CallFunctionObjArgs(loads, py_s, nullptr);
  Py_DECREF(loads);
  Py_DECREF(py_s);
  return out;
}

PyObject* il_flag(const char* op, const content::code::GisArg* args, int count, bool refresh) {
  std::string result;
  std::string error;
  if (!call_il(op, args, count, &result, &error) || result == "0") {
    Py_RETURN_FALSE;
  }
  if (refresh && content::gis_console_bridge().refresh_gis) {
    content::gis_console_bridge().refresh_gis();
  }
  Py_RETURN_TRUE;
}

}  // namespace detail
#endif
}  // namespace plugin
