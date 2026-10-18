// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/bind/forward.h"
#include "plugin/runtime/python/bind/gil.h"
#include "plugin/runtime/python/bind/py_compat.h"
#include "plugin/runtime/python/bind/slot.h"
#include "plugin/runtime/python/bind/surface.h"

#include "content/browser/glang.runtime/host/bind/gis_console.h"
#include "tool/command/command.h"

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

PyObject* tool_execute(PyObject*, PyObject* args, PyObject* kwargs) {
  const char* id = nullptr;
  unsigned int view_id = 0;
  const char* payload = "";
  static const char* kKw[] = {"id", "view_id", "payload", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|Is",
                                   const_cast<char**>(kKw), &id, &view_id,
                                   &payload)) {
    return nullptr;
  }
  if (!detail::bound_host()) {
    Py_RETURN_FALSE;
  }
  tool::CommandArgs ca;
  ca.view_id = view_id;
  ca.payload = payload ? payload : "";
  bool ran = false;
  {
    detail::ReleaseGil released;
    ran = detail::bound_host()->execute(id, ca);
  }
  if (!ran) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* tool_activate(PyObject*, PyObject* args, PyObject* kwargs) {
  const char* tool_id = nullptr;
  unsigned int view_id = 0;
  static const char* kKw[] = {"tool_id", "view_id", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|I",
                                   const_cast<char**>(kKw), &tool_id,
                                   &view_id)) {
    return nullptr;
  }
  if (!tool_id || !content::try_activate_tool(view_id, tool_id)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

// Command catalog surface, not glang.plugin verbs. See bind/forward.h.
PyMethodDef kToolMethods[] = {
    {"execute", reinterpret_cast<PyCFunction>(tool_execute),
     METH_VARARGS | METH_KEYWORDS, "Execute a command id."},
    {"activate", reinterpret_cast<PyCFunction>(tool_activate),
     METH_VARARGS | METH_KEYWORDS,
     "Activate an interactive tool (e.g. edit.append.linestring)."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kToolMod = {PyModuleDef_HEAD_INIT, "smartgis.tool", nullptr, -1,
                        kToolMethods};

}  // namespace

namespace detail {

PyObject* make_tool_module() {
  return PyModule_Create(&kToolMod);
}

}  // namespace detail
#endif
}  // namespace plugin
