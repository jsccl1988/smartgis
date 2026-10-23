// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"
#include "content/browser/glang.runtime/exec/py/embed/kernel/session.h"
#include "content/browser/glang.runtime/exec/py/embed/surface/surface.h"

#include "content/public/event_bus.h"

#include <cstdint>
#include <string>

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

PyObject* events_subscribe(PyObject*, PyObject* args) {
  const char* name = nullptr;
  PyObject* fn = nullptr;
  if (!PyArg_ParseTuple(args, "sO", &name, &fn)) {
    return nullptr;
  }
  content::PluginHost* host = detail::bound_host();
  if (!fn || !PyCallable_Check(fn) || !host || !host->events()) {
    PyErr_SetString(PyExc_RuntimeError, "events.subscribe needs a host bus");
    return nullptr;
  }
  const std::string event_name = name ? name : "";
  if (event_name != "SelectionChanged" && event_name != "ExtentChanged") {
    PyErr_SetString(PyExc_ValueError, "unknown event");
    return nullptr;
  }
  const std::uint64_t callback_id = detail::hold_callable(fn);
  if (callback_id == 0) {
    PyErr_SetString(PyExc_RuntimeError, "glang.plugin could not hold callback");
    return nullptr;
  }
  content::EventBus* bus = host->events();
  if (event_name == "SelectionChanged") {
    detail::hold_event(bus->subscribe<content::SelectionChanged>(
        [callback_id](const content::SelectionChanged&) {
          detail::call_py_void(callback_id);
        }));
  } else if (event_name == "ExtentChanged") {
    detail::hold_event(bus->subscribe<content::ExtentChanged>(
        [callback_id](const content::ExtentChanged&) {
          detail::call_py_void(callback_id);
        }));
  }
  Py_RETURN_NONE;
}

// Event bus surface, not a glang.plugin verb.
PyMethodDef kEventsMethods[] = {
    {"subscribe", events_subscribe, METH_VARARGS, "Subscribe to a domain event."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kEventsMod = {PyModuleDef_HEAD_INIT, "smartgis.content.events",
                          nullptr, -1, kEventsMethods};

}  // namespace

namespace detail {

PyObject* make_events_module() {
  return PyModule_Create(&kEventsMod);
}

}  // namespace detail
#endif
}  // namespace plugin
