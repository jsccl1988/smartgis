// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"
#include "content/browser/glang.runtime/exec/py/embed/kernel/session.h"
#include "content/browser/glang.runtime/exec/py/embed/surface/surface.h"

#include "base/trace/event/process_trace.h"
#include "plugin/runtime/host/capability/capability.h"
#include "ui/gis/debug/debug_console_panel.h"

#include <cstring>
#include <string>

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

PyObject* debug_set_tracing(PyObject*, PyObject* args) {
  int on = 0;
  if (!PyArg_ParseTuple(args, "p", &on)) {
    return nullptr;
  }
  base::trace::set_tracing_enabled(on != 0);
  Py_RETURN_NONE;
}

PyObject* debug_tracing_enabled(PyObject*, PyObject*) {
  if (base::trace::tracing_enabled()) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

PyObject* debug_show_tab(PyObject*, PyObject* args) {
  const char* name = nullptr;
  if (!PyArg_ParseTuple(args, "s", &name)) {
    return nullptr;
  }
  int tab = -1;
  if (name && std::strcmp(name, "output") == 0) {
    tab = 0;
  } else if (name && std::strcmp(name, "console") == 0) {
    tab = 1;
  } else if (name && std::strcmp(name, "trace") == 0) {
    tab = 2;
  } else if (name && std::strcmp(name, "memory") == 0) {
    tab = 3;
  }
  plugin::ShellUiSink* ui = plugin::shell_ui(detail::bound_host());
  if (!ui || tab < 0 || !ui->show_debug_tab(tab)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

// Context manager wrapping base::trace::ScopedTraceEvent (enter/exit).
struct TraceEventObject {
  PyObject_HEAD
  std::string* name;
  std::string* cat;
  base::trace::ScopedTraceEvent* event;
};

PyTypeObject* g_trace_event_type = nullptr;

void trace_event_dealloc(TraceEventObject* self) {
  delete self->event;
  self->event = nullptr;
  delete self->name;
  self->name = nullptr;
  delete self->cat;
  self->cat = nullptr;
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject* trace_event_enter(TraceEventObject* self, PyObject*) {
  if (!self->event && self->name && self->cat) {
    self->event = new base::trace::ScopedTraceEvent(*self->name, *self->cat);
  }
  Py_INCREF(self);
  return reinterpret_cast<PyObject*>(self);
}

PyObject* trace_event_exit(TraceEventObject* self, PyObject*) {
  delete self->event;
  self->event = nullptr;
  Py_RETURN_FALSE;
}

PyMethodDef kTraceEventMethods[] = {
    {"__enter__", reinterpret_cast<PyCFunction>(trace_event_enter), METH_NOARGS,
     nullptr},
    {"__exit__", reinterpret_cast<PyCFunction>(trace_event_exit), METH_VARARGS,
     nullptr},
    {nullptr, nullptr, 0, nullptr},
};

PyType_Slot kTraceEventSlots[] = {
    {Py_tp_dealloc, reinterpret_cast<void*>(trace_event_dealloc)},
    {Py_tp_methods, kTraceEventMethods},
    {0, nullptr},
};

PyType_Spec kTraceEventSpec = {
    .name = "smartgis.debug.TraceEvent",
    .basicsize = static_cast<int>(sizeof(TraceEventObject)),
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = kTraceEventSlots,
};

bool init_trace_event_type() {
  g_trace_event_type =
      reinterpret_cast<PyTypeObject*>(PyType_FromSpec(&kTraceEventSpec));
  return g_trace_event_type != nullptr;
}

PyObject* debug_trace_event(PyObject*, PyObject* args) {
  const char* name = nullptr;
  const char* cat = nullptr;
  if (!PyArg_ParseTuple(args, "ss", &name, &cat)) {
    return nullptr;
  }
  if (!g_trace_event_type && !init_trace_event_type()) {
    return nullptr;
  }
  PyObject* raw = g_trace_event_type->tp_alloc(g_trace_event_type, 0);
  if (!raw) {
    return nullptr;
  }
  auto* self = reinterpret_cast<TraceEventObject*>(raw);
  self->name = new std::string(name ? name : "");
  self->cat = new std::string(cat ? cat : "");
  self->event = nullptr;
  return raw;
}

// Trace console surface, not glang.plugin verbs.
PyMethodDef kDebugMethods[] = {
    {"set_tracing", debug_set_tracing, METH_VARARGS,
     "Enable or disable process-wide tracing."},
    {"tracing_enabled", debug_tracing_enabled, METH_NOARGS,
     "Whether process tracing is enabled."},
    {"trace_event", debug_trace_event, METH_VARARGS,
     "Context manager: base::trace::ScopedTraceEvent(name, cat)."},
    {"show_tab", debug_show_tab, METH_VARARGS,
     "show_tab(name) name=output|console|trace|memory."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kDebugMod = {PyModuleDef_HEAD_INIT, "smartgis.debug",
                         "Process-wide tracing for Diagnostic Tools.", -1,
                         kDebugMethods};

}  // namespace

namespace detail {

bool init_debug_type() {
  return init_trace_event_type();
}

PyObject* make_debug_module() {
  return PyModule_Create(&kDebugMod);
}

}  // namespace detail
#endif
}  // namespace plugin
