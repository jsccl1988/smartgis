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

#include "plugin/runtime/python/runtime.h"

#include "plugin/runtime/python/gis_bindings.h"

#include "base/trace/event/process_trace.h"
#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"
#include "ui/views/kernel/widget/widget.h"

#include <memory>
#include <string>
#include <string_view>
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

bool call_py_bool(PyObject* callable, const char* arg_or_null) {
  PyGILState_STATE gil = PyGILState_Ensure();
  PyObject* result =
      arg_or_null ? PyObject_CallFunction(callable, "s", arg_or_null)
                  : PyObject_CallFunction(callable, nullptr);
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
}

void call_py_void(PyObject* callable) {
  PyGILState_STATE gil = PyGILState_Ensure();
  PyObject* result = PyObject_CallFunction(callable, nullptr);
  if (!result) {
    PyErr_Print();
  } else {
    Py_DECREF(result);
  }
  PyGILState_Release(gil);
}

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
        return call_py_bool(callable, "");
      });
  if (!ok) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_contribute_dialog(HostObject* self, PyObject* args) {
  const char* plugin_id = nullptr;
  const char* dialog_id = nullptr;
  const char* title = nullptr;
  PyObject* callable = nullptr;
  if (!PyArg_ParseTuple(args, "sssO", &plugin_id, &dialog_id, &title,
                        &callable)) {
    return nullptr;
  }
  if (!self->host || !callable || !PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "contribute_dialog needs a callable");
    return nullptr;
  }
  hold_callable(callable);
  content::DialogContribution dlg;
  dlg.id = dialog_id ? dialog_id : "";
  dlg.title = title ? title : "";
  const bool ok = self->host->contribute_dialog(
      plugin_id, dlg, [callable](content::PluginHost*) { call_py_void(callable); });
  if (!ok) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_contribute_dock(HostObject* self, PyObject* args) {
  const char* plugin_id = nullptr;
  const char* dock_id = nullptr;
  const char* title = nullptr;
  const char* area = nullptr;
  PyObject* callable = nullptr;
  if (!PyArg_ParseTuple(args, "ssssO", &plugin_id, &dock_id, &title, &area,
                        &callable)) {
    return nullptr;
  }
  if (!self->host || !callable || !PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "contribute_dock needs a callable");
    return nullptr;
  }
  hold_callable(callable);
  content::DockContribution dock;
  dock.id = dock_id ? dock_id : "";
  dock.title = title ? title : "";
  dock.area = area ? area : "right";
  const bool ok = self->host->contribute_dock(
      plugin_id, dock, [callable](content::PluginHost*) { call_py_void(callable); });
  if (!ok) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_contribute_processing(HostObject* self, PyObject* args) {
  const char* plugin_id = nullptr;
  const char* proc_id = nullptr;
  const char* title = nullptr;
  PyObject* callable = nullptr;
  if (!PyArg_ParseTuple(args, "sssO", &plugin_id, &proc_id, &title, &callable)) {
    return nullptr;
  }
  if (!self->host || !callable || !PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "contribute_processing needs a callable");
    return nullptr;
  }
  hold_callable(callable);
  content::ProcessingContribution proc;
  proc.id = proc_id ? proc_id : "";
  proc.title = title ? title : "";
  const bool ok = self->host->contribute_processing(
      plugin_id, proc,
      [callable](content::PluginHost*, std::string_view args_json) {
        const std::string json(args_json);
        return call_py_bool(callable, json.c_str());
      });
  if (!ok) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_open_dialog(HostObject* self, PyObject* args) {
  const char* dialog_id = nullptr;
  if (!PyArg_ParseTuple(args, "s", &dialog_id)) {
    return nullptr;
  }
  if (!self->host || !dialog_id) {
    Py_RETURN_FALSE;
  }
  if (!self->host->open_dialog(dialog_id)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_open_report(HostObject* self, PyObject* args) {
  const char* path = nullptr;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return nullptr;
  }
  if (!self->host || !path) {
    Py_RETURN_FALSE;
  }
  if (!self->host->open_report(path)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_post_to_report(HostObject* self, PyObject* args) {
  const char* json = nullptr;
  if (!PyArg_ParseTuple(args, "s", &json)) {
    return nullptr;
  }
  if (!self->host || !json) {
    Py_RETURN_FALSE;
  }
  if (!self->host->post_to_report(json)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_close_report(HostObject* self, PyObject* /*args*/) {
  if (!self->host) {
    Py_RETURN_FALSE;
  }
  self->host->close_report();
  Py_RETURN_TRUE;
}

PyObject* host_run_processing(HostObject* self, PyObject* args) {
  const char* processing_id = nullptr;
  const char* args_json = "{}";
  if (!PyArg_ParseTuple(args, "s|s", &processing_id, &args_json)) {
    return nullptr;
  }
  if (!self->host || !processing_id) {
    Py_RETURN_FALSE;
  }
  if (!self->host->run_processing(processing_id, args_json ? args_json : "{}")) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyMethodDef kHostMethods[] = {
    {"contribute_command",
     reinterpret_cast<PyCFunction>(host_contribute_command), METH_VARARGS,
     "Contribute a command handler."},
    {"contribute_dialog", reinterpret_cast<PyCFunction>(host_contribute_dialog),
     METH_VARARGS, "Contribute a dialog factory."},
    {"contribute_dock", reinterpret_cast<PyCFunction>(host_contribute_dock),
     METH_VARARGS, "Contribute a dock factory."},
    {"contribute_processing",
     reinterpret_cast<PyCFunction>(host_contribute_processing), METH_VARARGS,
     "Contribute a processing factory (no Views)."},
    {"open_dialog", reinterpret_cast<PyCFunction>(host_open_dialog),
     METH_VARARGS, "Open a contributed dialog by id."},
    {"open_report", reinterpret_cast<PyCFunction>(host_open_report),
     METH_VARARGS, "Open a local HTML report directory in the Report dock."},
    {"post_to_report", reinterpret_cast<PyCFunction>(host_post_to_report),
     METH_VARARGS, "Post JSON to the open report (window message)."},
    {"close_report", reinterpret_cast<PyCFunction>(host_close_report),
     METH_NOARGS, "Close the Report dock document."},
    {"run_processing", reinterpret_cast<PyCFunction>(host_run_processing),
     METH_VARARGS, "Run a contributed processing id."},
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

PyObject* tool_activate(PyObject*, PyObject* args, PyObject* kwargs) {
  const char* tool_id = nullptr;
  unsigned int view_id = 0;
  static const char* kKw[] = {"tool_id", "view_id", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|I",
                                   const_cast<char**>(kKw), &tool_id,
                                   &view_id)) {
    return nullptr;
  }
  if (!tool_id || !try_activate_tool(view_id, tool_id)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

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

std::wstring filter_from_pattern(const char* pattern) {
  // OPENFILENAME wants label\0pattern\0 (trailing NUL from c_str()).
  const std::string pat = (pattern && pattern[0]) ? pattern : "*.*";
  std::wstring w;
  w.append(L"Files");
  w.push_back(L'\0');
  w.append(ui::views::utf8_to_wide(pat));
  w.push_back(L'\0');
  return w;
}

PyObject* picker_result_dict(const ui::views::FilePickerResult& r) {
  return Py_BuildValue("{s:N,s:s}", "accepted", PyBool_FromLong(r.accepted ? 1 : 0),
                       "path", r.path.c_str());
}

PyObject* ui_pick_open_file(PyObject*, PyObject* args) {
  const char* pattern = "*.*";
  if (!PyArg_ParseTuple(args, "|s", &pattern)) {
    return nullptr;
  }
  const std::wstring filter = filter_from_pattern(pattern);
  return picker_result_dict(ui::views::pick_open_file(filter.c_str()));
}

PyObject* ui_pick_save_file(PyObject*, PyObject* args) {
  const char* pattern = "*.*";
  if (!PyArg_ParseTuple(args, "|s", &pattern)) {
    return nullptr;
  }
  const std::wstring filter = filter_from_pattern(pattern);
  return picker_result_dict(ui::views::pick_save_file(filter.c_str()));
}

PyObject* ui_show_message_box(PyObject*, PyObject* args) {
  const char* kind = "info";
  const char* text = "";
  if (!PyArg_ParseTuple(args, "ss", &kind, &text)) {
    return nullptr;
  }
  ui::views::MessageBoxKind k = ui::views::MessageBoxKind::kInfo;
  if (kind && std::string(kind) == "error") {
    k = ui::views::MessageBoxKind::kError;
  }
  ui::views::show_message_box(k, text ? text : "");
  Py_RETURN_NONE;
}

PyMethodDef kToolMethods[] = {
    {"execute", reinterpret_cast<PyCFunction>(tool_execute),
     METH_VARARGS | METH_KEYWORDS, "Execute a command id."},
    {"activate", reinterpret_cast<PyCFunction>(tool_activate),
     METH_VARARGS | METH_KEYWORDS,
     "Activate an interactive tool (e.g. edit.append.linestring)."},
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
    {"pick_open_file", ui_pick_open_file, METH_VARARGS,
     "Open-file picker; returns {accepted, path}."},
    {"pick_save_file", ui_pick_save_file, METH_VARARGS,
     "Save-file picker; returns {accepted, path}."},
    {"show_message_box", ui_show_message_box, METH_VARARGS,
     "show_message_box(kind, text) kind=info|error."},
    {nullptr, nullptr, 0, nullptr},
};

PyObject* config_themes(PyObject*, PyObject*) {
  auto& svc = ui::views::ThemeService::get();
  svc.ensure_builtin_packs();
  PyObject* list = PyList_New(0);
  if (!list) {
    return nullptr;
  }
  for (const ui::views::ThemePack& pack : svc.packs()) {
    PyObject* item = Py_BuildValue("{s:s,s:s}", "id", pack.id.c_str(), "label",
                                   pack.label.c_str());
    if (!item || PyList_Append(list, item) < 0) {
      Py_XDECREF(item);
      Py_DECREF(list);
      return nullptr;
    }
    Py_DECREF(item);
  }
  return list;
}

PyObject* config_theme_id(PyObject*, PyObject*) {
  auto& svc = ui::views::ThemeService::get();
  svc.ensure_builtin_packs();
  const std::string id(svc.current_id());
  return PyUnicode_FromString(id.c_str());
}

PyObject* config_set_theme(PyObject*, PyObject* args) {
  const char* id = nullptr;
  if (!PyArg_ParseTuple(args, "s", &id)) {
    return nullptr;
  }
  auto& svc = ui::views::ThemeService::get();
  svc.ensure_builtin_packs();
  if (!svc.set_theme(id ? id : "")) {
    Py_RETURN_FALSE;
  }
  svc.persist();
  Py_RETURN_TRUE;
}

PyMethodDef kConfigMethods[] = {
    {"themes", config_themes, METH_NOARGS, "Registered UI theme packs."},
    {"theme_id", config_theme_id, METH_NOARGS, "Active theme pack id."},
    {"set_theme", config_set_theme, METH_VARARGS, "Set + persist theme id."},
    {nullptr, nullptr, 0, nullptr},
};

PyMethodDef kDebugMethods[] = {
    {"set_tracing", debug_set_tracing, METH_VARARGS,
     "Enable or disable process-wide tracing."},
    {"tracing_enabled", debug_tracing_enabled, METH_NOARGS,
     "Whether process tracing is enabled."},
    {"trace_event", debug_trace_event, METH_VARARGS,
     "Context manager: base::trace::ScopedTraceEvent(name, cat)."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kToolMod = {PyModuleDef_HEAD_INIT, "smartgis.tool", nullptr, -1,
                        kToolMethods};
PyModuleDef kEventsMod = {PyModuleDef_HEAD_INIT, "smartgis.content.events",
                          nullptr, -1, kEventsMethods};
PyModuleDef kUiMod = {PyModuleDef_HEAD_INIT, "smartgis.ui", nullptr, -1,
                      kUiMethods};
PyModuleDef kConfigMod = {PyModuleDef_HEAD_INIT, "smartgis.ui.config",
                          "Shell ThemeService / system UI config.", -1,
                          kConfigMethods};
PyModuleDef kDebugMod = {PyModuleDef_HEAD_INIT, "smartgis.debug",
                         "Process-wide tracing for Diagnostic Tools.", -1,
                         kDebugMethods};
PyModuleDef kContentMod = {PyModuleDef_HEAD_INIT, "smartgis.content", nullptr, -1,
                           nullptr};
PyModuleDef kSmartgisMod = {PyModuleDef_HEAD_INIT, "smartgis", nullptr, -1,
                            nullptr};

PyObject* PyInit_smartgis_impl() {
  if (!init_host_type()) {
    return nullptr;
  }
  if (!init_trace_event_type()) {
    return nullptr;
  }
  PyObject* m = PyModule_Create(&kSmartgisMod);
  PyObject* content = PyModule_Create(&kContentMod);
  PyObject* events = PyModule_Create(&kEventsMod);
  PyObject* tool = PyModule_Create(&kToolMod);
  PyObject* ui = PyModule_Create(&kUiMod);
  PyObject* config = PyModule_Create(&kConfigMod);
  PyObject* debug = PyModule_Create(&kDebugMod);
  if (!m || !content || !events || !tool || !ui || !config || !debug) {
    return nullptr;
  }
  Py_INCREF(reinterpret_cast<PyObject*>(g_host_type));
  PyModule_AddObject(m, "Host", reinterpret_cast<PyObject*>(g_host_type));
  PyModule_AddObject(content, "events", events);
  Py_INCREF(Py_None);
  PyModule_AddObject(content, "host", Py_None);
  PyModule_AddObject(m, "content", content);
  PyModule_AddObject(m, "tool", tool);
  if (PyModule_AddObject(ui, "config", config) < 0) {
    Py_DECREF(config);
    return nullptr;
  }
  PyModule_AddObject(m, "ui", ui);
  if (PyModule_AddObject(m, "debug", debug) < 0) {
    Py_DECREF(debug);
    return nullptr;
  }
  if (!attach_gis_module(m)) {
    return nullptr;
  }
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
  bind_gis_host_for_analysis(host);
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
  bind_gis_host_for_analysis(nullptr);
}
#endif

void register_smartgis_bindings() {
#if defined(SMT_HAS_PYTHON)
  // Module is registered via PyImport_AppendInittab in PythonRuntime::init.
#endif
}

}  // namespace plugin
