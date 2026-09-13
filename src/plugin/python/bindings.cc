// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#if defined(SMT_HAS_PYTHON)
#define PY_SSIZE_T_CLEAN
#ifdef _DEBUG
#define SMT_PYTHON_RESTORE_DEBUG
#undef _DEBUG
#endif
#include <Python.h>
#ifdef SMT_PYTHON_RESTORE_DEBUG
#define _DEBUG
#undef SMT_PYTHON_RESTORE_DEBUG
#endif
#endif

#include "plugin/python/runtime.h"

#include "content/public/event_bus.h"
#include "content/public/events.h"
#include "content/public/plugin_host.h"
#include "tool/command.h"
#include "ui/views/button.h"
#include "ui/views/checkbox.h"
#include "ui/views/label.h"
#include "ui/views/textfield.h"
#include "ui/views/widget.h"

#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace plugin {
namespace {

#if defined(SMT_HAS_PYTHON)

content::PluginHost* g_bound_host = nullptr;
std::vector<PyObject*> g_held_callables;
std::vector<content::EventBus::Connection> g_event_conns;

void hold_callable(PyObject* obj) {
  if (obj) {
    Py_INCREF(obj);
    g_held_callables.push_back(obj);
  }
}

void drop_held_callables() {
  for (PyObject* obj : g_held_callables) {
    Py_DECREF(obj);
  }
  g_held_callables.clear();
  g_event_conns.clear();
}

struct HostObject {
  PyObject_HEAD
  content::PluginHost* host;
};

PyObject* host_contribute_command(HostObject* self, PyObject* args) {
  const char* plugin_id = nullptr;
  const char* command_id = nullptr;
  const char* title = nullptr;
  const char* menu = nullptr;
  PyObject* callable = nullptr;
  if (!PyArg_ParseTuple(args, "ssssO", &plugin_id, &command_id, &title, &menu,
                        &callable)) {
    return nullptr;
  }
  if (!self->host || !callable || !PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "contribute_command needs a callable");
    return nullptr;
  }
  hold_callable(callable);
  const bool ok = self->host->contribute_command(
      plugin_id, command_id, title, menu,
      [callable](const tool::CommandArgs&) {
        PyGILState_STATE gil = PyGILState_Ensure();
        PyObject* result = PyObject_CallFunction(callable, "s", "");
        bool pass = true;
        if (!result) {
          PyErr_Print();
          pass = false;
        } else {
          pass = PyObject_IsTrue(result) != 0;
          Py_DECREF(result);
        }
        PyGILState_Release(gil);
        return pass;
      });
  if (!ok) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyMethodDef kHostMethods[] = {
    {"contribute_command", reinterpret_cast<PyCFunction>(host_contribute_command),
     METH_VARARGS, "Contribute a command handler."},
    {nullptr, nullptr, 0, nullptr},
};

PyTypeObject* g_host_type = nullptr;

int host_init(PyObject* self, PyObject*, PyObject*) {
  reinterpret_cast<HostObject*>(self)->host = nullptr;
  return 0;
}

PyType_Slot kHostSlots[] = {
    {Py_tp_methods, kHostMethods},
    {Py_tp_init, reinterpret_cast<void*>(host_init)},
    {0, nullptr},
};

PyType_Spec kHostSpec = {
    .name = "smartgis.Host",
    .basicsize = static_cast<int>(sizeof(HostObject)),
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = kHostSlots,
};

bool init_host_type() {
  g_host_type = reinterpret_cast<PyTypeObject*>(PyType_FromSpec(&kHostSpec));
  return g_host_type != nullptr;
}

PyObject* make_host_object(content::PluginHost* host) {
  if (!g_host_type) {
    return nullptr;
  }
  PyObject* raw = PyObject_CallNoArgs(reinterpret_cast<PyObject*>(g_host_type));
  if (!raw) {
    return nullptr;
  }
  reinterpret_cast<HostObject*>(raw)->host = host;
  return raw;
}

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
  if (!g_bound_host) {
    Py_RETURN_FALSE;
  }
  tool::CommandArgs ca;
  ca.view_id = view_id;
  ca.payload = payload ? payload : "";
  if (!g_bound_host->execute(id, ca)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* events_subscribe(PyObject*, PyObject* args) {
  const char* name = nullptr;
  PyObject* fn = nullptr;
  if (!PyArg_ParseTuple(args, "sO", &name, &fn)) {
    return nullptr;
  }
  if (!fn || !PyCallable_Check(fn) || !g_bound_host || !g_bound_host->events()) {
    PyErr_SetString(PyExc_RuntimeError, "events.subscribe needs a host bus");
    return nullptr;
  }
  hold_callable(fn);
  content::EventBus* bus = g_bound_host->events();
  if (std::string(name) == "SelectionChanged") {
    g_event_conns.push_back(bus->subscribe<content::SelectionChanged>(
        [fn](const content::SelectionChanged&) {
          PyGILState_STATE gil = PyGILState_Ensure();
          PyObject* r = PyObject_CallFunction(fn, "");
          Py_XDECREF(r);
          if (PyErr_Occurred()) {
            PyErr_Print();
          }
          PyGILState_Release(gil);
        }));
  } else if (std::string(name) == "ExtentChanged") {
    g_event_conns.push_back(bus->subscribe<content::ExtentChanged>(
        [fn](const content::ExtentChanged&) {
          PyGILState_STATE gil = PyGILState_Ensure();
          PyObject* r = PyObject_CallFunction(fn, "");
          Py_XDECREF(r);
          if (PyErr_Occurred()) {
            PyErr_Print();
          }
          PyGILState_Release(gil);
        }));
  } else {
    PyErr_SetString(PyExc_ValueError, "unknown event");
    return nullptr;
  }
  Py_RETURN_NONE;
}

template <typename Cpp, typename... Args>
PyObject* wrap_new(Args&&... args) {
  auto* p = new Cpp(std::forward<Args>(args)...);
  return PyCapsule_New(p, typeid(Cpp).name(), [](PyObject* cap) {
    delete static_cast<Cpp*>(PyCapsule_GetPointer(cap, typeid(Cpp).name()));
  });
}

PyObject* ui_label(PyObject*, PyObject* args) {
  const char* text = "";
  if (!PyArg_ParseTuple(args, "|s", &text)) {
    return nullptr;
  }
  return wrap_new<ui::views::Label>(std::string(text ? text : ""));
}

PyObject* ui_button(PyObject*, PyObject* args) {
  const char* text = "";
  if (!PyArg_ParseTuple(args, "|s", &text)) {
    return nullptr;
  }
  return wrap_new<ui::views::Button>(std::string(text ? text : ""));
}

PyObject* ui_textfield(PyObject*, PyObject*) {
  return wrap_new<ui::views::Textfield>();
}

PyObject* ui_checkbox(PyObject*, PyObject* args) {
  const char* text = "";
  if (!PyArg_ParseTuple(args, "|s", &text)) {
    return nullptr;
  }
  return wrap_new<ui::views::Checkbox>(std::string(text ? text : ""));
}

PyObject* ui_widget(PyObject*, PyObject*) {
  return wrap_new<ui::views::Widget>();
}

PyMethodDef kToolMethods[] = {
    {"execute", reinterpret_cast<PyCFunction>(tool_execute),
     METH_VARARGS | METH_KEYWORDS, "Execute a command id."},
    {nullptr, nullptr, 0, nullptr},
};

PyMethodDef kEventsMethods[] = {
    {"subscribe", events_subscribe, METH_VARARGS, "Subscribe to a domain event."},
    {nullptr, nullptr, 0, nullptr},
};

PyMethodDef kUiMethods[] = {
    {"Label", ui_label, METH_VARARGS, nullptr},
    {"Button", ui_button, METH_VARARGS, nullptr},
    {"Textfield", ui_textfield, METH_NOARGS, nullptr},
    {"Checkbox", ui_checkbox, METH_VARARGS, nullptr},
    {"Widget", ui_widget, METH_NOARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kToolMod = {PyModuleDef_HEAD_INIT, "smartgis.tool", nullptr, -1,
                        kToolMethods};
PyModuleDef kEventsMod = {PyModuleDef_HEAD_INIT, "smartgis.content.events",
                          nullptr, -1, kEventsMethods};
PyModuleDef kUiMod = {PyModuleDef_HEAD_INIT, "smartgis.ui", nullptr, -1,
                      kUiMethods};
PyModuleDef kContentMod = {PyModuleDef_HEAD_INIT, "smartgis.content", nullptr, -1,
                           nullptr};
PyModuleDef kSmartgisMod = {PyModuleDef_HEAD_INIT, "smartgis", nullptr, -1,
                            nullptr};

PyObject* PyInit_smartgis_impl() {
  if (!init_host_type()) {
    return nullptr;
  }
  PyObject* m = PyModule_Create(&kSmartgisMod);
  PyObject* content = PyModule_Create(&kContentMod);
  PyObject* events = PyModule_Create(&kEventsMod);
  PyObject* tool = PyModule_Create(&kToolMod);
  PyObject* ui = PyModule_Create(&kUiMod);
  if (!m || !content || !events || !tool || !ui) {
    return nullptr;
  }
  Py_INCREF(reinterpret_cast<PyObject*>(g_host_type));
  PyModule_AddObject(m, "Host", reinterpret_cast<PyObject*>(g_host_type));
  PyModule_AddObject(content, "events", events);
  Py_INCREF(Py_None);
  PyModule_AddObject(content, "host", Py_None);
  PyModule_AddObject(m, "content", content);
  PyModule_AddObject(m, "tool", tool);
  PyModule_AddObject(m, "ui", ui);
  return m;
}

#endif  // SMT_HAS_PYTHON

}  // namespace

#if defined(SMT_HAS_PYTHON)
extern "C" PyObject* PyInit_smartgis() {
  return PyInit_smartgis_impl();
}

PyObject* make_python_host(content::PluginHost* host) {
  return make_host_object(host);
}

void bind_python_host(content::PluginHost* host) {
  g_bound_host = host;
  PyObject* smartgis = PyImport_ImportModule("smartgis");
  if (!smartgis) {
    return;
  }
  PyObject* content = PyObject_GetAttrString(smartgis, "content");
  PyObject* wrapper = make_host_object(host);
  if (content && wrapper) {
    PyObject_SetAttrString(content, "host", wrapper);
  }
  Py_XDECREF(wrapper);
  Py_XDECREF(content);
  Py_DECREF(smartgis);
}

void unbind_python_host() {
  drop_held_callables();
  g_bound_host = nullptr;
}
#endif

void register_smartgis_bindings() {
#if defined(SMT_HAS_PYTHON)
  // Module is registered via PyImport_AppendInittab in PythonRuntime::init.
#endif
}

}  // namespace plugin
