// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#if defined(HAS_PYTHON)
#define PY_SSIZE_T_CLEAN
#ifdef _DEBUG
#define PYTHON_RESTORE_DEBUG
#undef _DEBUG
#endif
#include <Python.h>
#ifdef PYTHON_RESTORE_DEBUG
#define _DEBUG
#undef PYTHON_RESTORE_DEBUG
#endif
#endif

#include "plugin/runtime/python/runtime.h"

#include "plugin/runtime/python/gis_bindings.h"

#include "base/trace/event/process_trace.h"
#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "tool/command/command.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/inspect/measure_panel.h"
#include "ui/gis/inspect/selection_panel.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"
#include "ui/views/kernel/widget/widget.h"

#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

namespace plugin {
namespace {

#if defined(HAS_PYTHON)

content::PluginHost* g_bound_host = nullptr;
std::string g_loading_plugin_dir;

struct HeldCallable {
  PyObject* obj = nullptr;
  std::string dir;
};
std::vector<HeldCallable> g_held_callables;

struct HeldEvent {
  content::EventBus::Connection conn;
  std::string dir;
  HeldEvent(content::EventBus::Connection c, std::string d)
      : conn(std::move(c)), dir(std::move(d)) {}
};
std::vector<HeldEvent> g_event_conns;

void hold_callable(PyObject* obj) {
  if (obj) {
    Py_INCREF(obj);
    g_held_callables.push_back(HeldCallable{obj, g_loading_plugin_dir});
  }
}

void drop_held_callables() {
  for (HeldCallable& h : g_held_callables) {
    Py_DECREF(h.obj);
  }
  g_held_callables.clear();
  g_event_conns.clear();
}

void drop_held_callables_for(std::string_view directory) {
  const std::string dir(directory);
  auto it = g_held_callables.begin();
  while (it != g_held_callables.end()) {
    if (it->dir == dir) {
      Py_DECREF(it->obj);
      it = g_held_callables.erase(it);
    } else {
      ++it;
    }
  }
  auto ev = g_event_conns.begin();
  while (ev != g_event_conns.end()) {
    if (ev->dir == dir) {
      ev = g_event_conns.erase(ev);
    } else {
      ++ev;
    }
  }
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
      [callable](const tool::CommandArgs& ca) {
        const std::string payload(ca.payload);
        return call_py_bool(callable, payload.c_str());
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

PyObject* host_open_dock(HostObject* self, PyObject* args) {
  const char* dock_id = nullptr;
  if (!PyArg_ParseTuple(args, "s", &dock_id)) {
    return nullptr;
  }
  if (!self->host || !dock_id) {
    Py_RETURN_FALSE;
  }
  if (!self->host->open_dock(dock_id)) {
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
  plugin::ReportBridge* report = plugin::report_bridge(self->host);
  if (!report || !report->open(path)) {
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
  plugin::ReportBridge* report = plugin::report_bridge(self->host);
  if (!report || !report->post(json)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_close_report(HostObject* self, PyObject* /*args*/) {
  if (!self->host) {
    Py_RETURN_FALSE;
  }
  plugin::ReportBridge* report = plugin::report_bridge(self->host);
  if (!report) {
    Py_RETURN_FALSE;
  }
  report->close();
  Py_RETURN_TRUE;
}

PyObject* host_present_dataset(HostObject* self, PyObject* args) {
  const char* plugin_id = nullptr;
  const char* path = "";
  int face = 0;
  int surface = -1;
  if (!PyArg_ParseTuple(args, "s|sii", &plugin_id, &path, &face, &surface)) {
    return nullptr;
  }
  if (!self->host || !plugin_id) {
    Py_RETURN_FALSE;
  }
  const bool ok =
      surface < 0
          ? self->host->present_dataset(plugin_id, path ? path : "", face)
          : self->host->present_dataset(plugin_id, path ? path : "", face,
                                        surface);
  if (!ok) {
    Py_RETURN_FALSE;
  }
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

PyObject* host_execute(HostObject* self, PyObject* args) {
  const char* command_id = nullptr;
  const char* payload = "";
  unsigned int view_id = 0;
  if (!PyArg_ParseTuple(args, "s|sI", &command_id, &payload, &view_id)) {
    return nullptr;
  }
  if (!self->host || !command_id) {
    Py_RETURN_FALSE;
  }
  tool::CommandArgs ca;
  ca.view_id = view_id;
  ca.payload = payload ? payload : "";
  if (!self->host->execute(command_id, ca)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_withdraw(HostObject* self, PyObject* args) {
  const char* plugin_id = nullptr;
  if (!PyArg_ParseTuple(args, "s", &plugin_id)) {
    return nullptr;
  }
  if (!self->host || !plugin_id) {
    Py_RETURN_FALSE;
  }
  self->host->withdraw(plugin_id);
  Py_RETURN_TRUE;
}

PyObject* host_contribute_menu(HostObject* self, PyObject* args) {
  const char* plugin_id = nullptr;
  const char* menu_id = nullptr;
  const char* title = nullptr;
  const char* parent = "tools";
  if (!PyArg_ParseTuple(args, "sss|s", &plugin_id, &menu_id, &title, &parent)) {
    return nullptr;
  }
  if (!self->host) {
    Py_RETURN_FALSE;
  }
  content::MenuContribution menu;
  menu.id = menu_id ? menu_id : "";
  menu.title = title ? title : "";
  menu.parent = parent ? parent : "tools";
  if (!self->host->contribute_menu(plugin_id, menu)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_contribute_export_frame(HostObject* self, PyObject* args) {
  const char* plugin_id = nullptr;
  const char* frame_id = nullptr;
  double min_lon = 0, min_lat = 0, max_lon = 0, max_lat = 0;
  if (!PyArg_ParseTuple(args, "ssdddd", &plugin_id, &frame_id, &min_lon,
                        &min_lat, &max_lon, &max_lat)) {
    return nullptr;
  }
  if (!self->host) {
    Py_RETURN_FALSE;
  }
  content::ExportFrameContribution frame;
  frame.id = frame_id ? frame_id : "";
  frame.min_lon = min_lon;
  frame.min_lat = min_lat;
  frame.max_lon = max_lon;
  frame.max_lat = max_lat;
  if (!self->host->contribute_export_frame(plugin_id, frame)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_lookup_export_frame(HostObject* self, PyObject* args) {
  const char* frame_id = nullptr;
  if (!PyArg_ParseTuple(args, "s", &frame_id)) {
    return nullptr;
  }
  if (!self->host || !frame_id) {
    Py_RETURN_NONE;
  }
  double min_lon = 0, min_lat = 0, max_lon = 0, max_lat = 0;
  if (!self->host->lookup_export_frame(frame_id, &min_lon, &min_lat, &max_lon,
                                       &max_lat)) {
    Py_RETURN_NONE;
  }
  return Py_BuildValue("(dddd)", min_lon, min_lat, max_lon, max_lat);
}

PyObject* host_list_contributions(HostObject* self, PyObject* args) {
  const char* kind = "command";
  if (!PyArg_ParseTuple(args, "|s", &kind)) {
    return nullptr;
  }
  if (!self->host) {
    return PyList_New(0);
  }
  PyObject* list = PyList_New(0);
  if (!list) {
    return nullptr;
  }
  auto append = [list](std::string_view plugin_id, std::string_view id,
                       std::string_view title) {
    PyObject* item =
        Py_BuildValue("{s:s,s:s,s:s}", "plugin_id",
                      std::string(plugin_id).c_str(), "id",
                      std::string(id).c_str(), "title",
                      std::string(title).c_str());
    if (item) {
      PyList_Append(list, item);
      Py_DECREF(item);
    }
  };
  const std::string k = kind ? kind : "command";
  if (k == "processing") {
    self->host->for_each_processing(append);
  } else if (k == "dialog") {
    self->host->for_each_dialog(append);
  } else if (k == "dock") {
    self->host->for_each_dock(append);
  } else {
    self->host->for_each_command(append);
  }
  return list;
}

PyObject* host_set_present_surface(HostObject* self, PyObject* args) {
  int surface = 0;
  if (!PyArg_ParseTuple(args, "i", &surface)) {
    return nullptr;
  }
  if (!self->host) {
    Py_RETURN_FALSE;
  }
  self->host->set_present_surface(surface);
  Py_RETURN_TRUE;
}

PyObject* host_present_surface(HostObject* self, PyObject*) {
  if (!self->host) {
    return PyLong_FromLong(0);
  }
  return PyLong_FromLong(self->host->present_surface());
}

PyObject* host_has_capability(HostObject* self, PyObject* args) {
  const char* id = nullptr;
  if (!PyArg_ParseTuple(args, "s", &id)) {
    return nullptr;
  }
  if (!self->host || !id || !self->host->query_capability(id)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_playback_push(HostObject* self, PyObject* args) {
  const char* json = nullptr;
  if (!PyArg_ParseTuple(args, "s", &json)) {
    return nullptr;
  }
  content::PluginHost::Playback* pb =
      self->host ? self->host->playback() : nullptr;
  if (!pb) {
    Py_RETURN_FALSE;
  }
  pb->push_frame(json ? json : "");
  Py_RETURN_TRUE;
}

PyObject* host_playback_count(HostObject* self, PyObject*) {
  content::PluginHost::Playback* pb =
      self->host ? self->host->playback() : nullptr;
  return PyLong_FromSize_t(pb ? pb->frame_count() : 0);
}

PyObject* host_playback_set_index(HostObject* self, PyObject* args) {
  unsigned long i = 0;
  if (!PyArg_ParseTuple(args, "k", &i)) {
    return nullptr;
  }
  content::PluginHost::Playback* pb =
      self->host ? self->host->playback() : nullptr;
  if (!pb) {
    Py_RETURN_FALSE;
  }
  pb->set_index(static_cast<size_t>(i));
  Py_RETURN_TRUE;
}

PyObject* host_playback_frame(HostObject* self, PyObject* args) {
  unsigned long i = 0;
  if (!PyArg_ParseTuple(args, "k", &i)) {
    return nullptr;
  }
  content::PluginHost::Playback* pb =
      self->host ? self->host->playback() : nullptr;
  if (!pb) {
    Py_RETURN_NONE;
  }
  const std::string_view json = pb->frame_json(static_cast<size_t>(i));
  if (json.empty()) {
    Py_RETURN_NONE;
  }
  return PyUnicode_FromStringAndSize(json.data(),
                                     static_cast<Py_ssize_t>(json.size()));
}

PyObject* host_playback_clear(HostObject* self, PyObject*) {
  content::PluginHost::Playback* pb =
      self->host ? self->host->playback() : nullptr;
  if (!pb) {
    Py_RETURN_FALSE;
  }
  pb->clear();
  Py_RETURN_TRUE;
}

PyObject* sink_json_result(bool ok, const std::string& json) {
  return Py_BuildValue("{s:N,s:s}", "ok", PyBool_FromLong(ok ? 1 : 0), "json",
                       json.c_str());
}

PyObject* host_scene3d_open_earth(HostObject* self, PyObject*) {
  plugin::Scene3dSink* sink = plugin::scene3d_sink(self->host);
  if (!sink || !sink->open_earth()) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_scene3d_fly_to(HostObject* self, PyObject* args) {
  double lon = 0, lat = 0, span = 10;
  float distance = 1.2f;
  if (!PyArg_ParseTuple(args, "dd|fd", &lon, &lat, &distance, &span)) {
    return nullptr;
  }
  plugin::Scene3dSink* sink = plugin::scene3d_sink(self->host);
  if (!sink || !sink->fly_to(lon, lat, distance, span)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_scene3d_apply_look(HostObject* self, PyObject* args) {
  const char* mode = nullptr;
  if (!PyArg_ParseTuple(args, "s", &mode)) {
    return nullptr;
  }
  plugin::Scene3dSink* sink = plugin::scene3d_sink(self->host);
  std::string json;
  const bool ok = sink && sink->apply_look(mode ? mode : "", &json);
  return sink_json_result(ok, json);
}

PyObject* host_scene3d_set_atmosphere(HostObject* self, PyObject* args) {
  int sky = 1, ocean = 1, cloud = 0, fog = 0;
  if (!PyArg_ParseTuple(args, "|pppp", &sky, &ocean, &cloud, &fog)) {
    return nullptr;
  }
  plugin::Scene3dSink* sink = plugin::scene3d_sink(self->host);
  if (!sink || !sink->set_atmosphere(sky != 0, ocean != 0, cloud != 0, fog != 0)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_scene3d_invalidate(HostObject* self, PyObject*) {
  plugin::Scene3dSink* sink = plugin::scene3d_sink(self->host);
  if (!sink) {
    Py_RETURN_FALSE;
  }
  sink->invalidate();
  Py_RETURN_TRUE;
}

PyObject* host_map2d_open_map(HostObject* self, PyObject*) {
  plugin::Map2dSink* sink = plugin::map2d_sink(self->host);
  if (!sink || !sink->open_map()) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_map2d_frame_to(HostObject* self, PyObject* args) {
  double lon = 0, lat = 0, span = 10;
  if (!PyArg_ParseTuple(args, "ddd", &lon, &lat, &span)) {
    return nullptr;
  }
  plugin::Map2dSink* sink = plugin::map2d_sink(self->host);
  if (!sink || !sink->frame_to(lon, lat, span)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_map2d_apply_look(HostObject* self, PyObject* args) {
  const char* mode = nullptr;
  if (!PyArg_ParseTuple(args, "s", &mode)) {
    return nullptr;
  }
  plugin::Map2dSink* sink = plugin::map2d_sink(self->host);
  std::string json;
  const bool ok = sink && sink->apply_look(mode ? mode : "", &json);
  return sink_json_result(ok, json);
}

PyObject* host_map2d_export_bmp(HostObject* self, PyObject* args) {
  const char* path = nullptr;
  int w = 1280, h = 720;
  if (!PyArg_ParseTuple(args, "s|ii", &path, &w, &h)) {
    return nullptr;
  }
  plugin::Map2dSink* sink = plugin::map2d_sink(self->host);
  if (!sink || !sink->export_bmp(path ? path : "", w, h)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* host_map2d_invalidate(HostObject* self, PyObject*) {
  plugin::Map2dSink* sink = plugin::map2d_sink(self->host);
  if (!sink) {
    Py_RETURN_FALSE;
  }
  sink->invalidate();
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
    {"contribute_menu", reinterpret_cast<PyCFunction>(host_contribute_menu),
     METH_VARARGS, "Contribute a menu node."},
    {"contribute_export_frame",
     reinterpret_cast<PyCFunction>(host_contribute_export_frame), METH_VARARGS,
     "Contribute a named Map2d export extent."},
    {"lookup_export_frame",
     reinterpret_cast<PyCFunction>(host_lookup_export_frame), METH_VARARGS,
     "Lookup export frame (min_lon, min_lat, max_lon, max_lat) or None."},
    {"execute", reinterpret_cast<PyCFunction>(host_execute), METH_VARARGS,
     "execute(command_id, payload='', view_id=0)."},
    {"withdraw", reinterpret_cast<PyCFunction>(host_withdraw), METH_VARARGS,
     "Withdraw all contributions for a plugin id."},
    {"list_contributions",
     reinterpret_cast<PyCFunction>(host_list_contributions), METH_VARARGS,
     "list_contributions(kind='command'|'processing'|'dialog'|'dock')."},
    {"set_present_surface",
     reinterpret_cast<PyCFunction>(host_set_present_surface), METH_VARARGS,
     "Sticky present surface 0=main 1=preview."},
    {"present_surface", reinterpret_cast<PyCFunction>(host_present_surface),
     METH_NOARGS, "Sticky present surface."},
    {"has_capability", reinterpret_cast<PyCFunction>(host_has_capability),
     METH_VARARGS, "True if query_capability(id) is set."},
    {"playback_push", reinterpret_cast<PyCFunction>(host_playback_push),
     METH_VARARGS, "Push a playback JSON frame."},
    {"playback_count", reinterpret_cast<PyCFunction>(host_playback_count),
     METH_NOARGS, "Playback frame count."},
    {"playback_set_index",
     reinterpret_cast<PyCFunction>(host_playback_set_index), METH_VARARGS,
     "Select playback frame index."},
    {"playback_frame", reinterpret_cast<PyCFunction>(host_playback_frame),
     METH_VARARGS, "JSON at playback index or None."},
    {"playback_clear", reinterpret_cast<PyCFunction>(host_playback_clear),
     METH_NOARGS, "Clear playback frames."},
    {"scene3d_open_earth",
     reinterpret_cast<PyCFunction>(host_scene3d_open_earth), METH_NOARGS,
     "Open True-Earth via plugin.scene3d."},
    {"scene3d_fly_to", reinterpret_cast<PyCFunction>(host_scene3d_fly_to),
     METH_VARARGS, "scene3d_fly_to(lon, lat, distance=1.2, span=10)."},
    {"scene3d_apply_look",
     reinterpret_cast<PyCFunction>(host_scene3d_apply_look), METH_VARARGS,
     "Returns {ok, json}."},
    {"scene3d_set_atmosphere",
     reinterpret_cast<PyCFunction>(host_scene3d_set_atmosphere), METH_VARARGS,
     "scene3d_set_atmosphere(sky, ocean, cloud, fog)."},
    {"scene3d_invalidate",
     reinterpret_cast<PyCFunction>(host_scene3d_invalidate), METH_NOARGS,
     "Invalidate Scene3D present."},
    {"map2d_open_map", reinterpret_cast<PyCFunction>(host_map2d_open_map),
     METH_NOARGS, "Open Map2d via plugin.map2d."},
    {"map2d_frame_to", reinterpret_cast<PyCFunction>(host_map2d_frame_to),
     METH_VARARGS, "map2d_frame_to(lon, lat, span_deg)."},
    {"map2d_apply_look", reinterpret_cast<PyCFunction>(host_map2d_apply_look),
     METH_VARARGS, "Returns {ok, json}."},
    {"map2d_export_bmp", reinterpret_cast<PyCFunction>(host_map2d_export_bmp),
     METH_VARARGS, "map2d_export_bmp(path, width=1280, height=720)."},
    {"map2d_invalidate", reinterpret_cast<PyCFunction>(host_map2d_invalidate),
     METH_NOARGS, "Invalidate Map2d present."},
    {"open_dialog", reinterpret_cast<PyCFunction>(host_open_dialog),
     METH_VARARGS, "Open a contributed dialog by id."},
    {"open_dock", reinterpret_cast<PyCFunction>(host_open_dock), METH_VARARGS,
     "Open a contributed dock by id (chrome mounts inspector pages)."},
    {"open_report", reinterpret_cast<PyCFunction>(host_open_report),
     METH_VARARGS, "Open a local HTML report directory in the Report dock."},
    {"post_to_report", reinterpret_cast<PyCFunction>(host_post_to_report),
     METH_VARARGS, "Post JSON to the open report (window message)."},
    {"close_report", reinterpret_cast<PyCFunction>(host_close_report),
     METH_NOARGS, "Close the Report dock document."},
    {"present_dataset", reinterpret_cast<PyCFunction>(host_present_dataset),
     METH_VARARGS,
     "Show plugin sample: face 0=Map/1=Scene3D; optional surface 0=main/1=preview."},
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
  plugin::ShellUiSink* ui = plugin::shell_ui(g_bound_host);
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

PyObject* debug_profile(PyObject* self, PyObject* args) {
  const char* name = nullptr;
  const char* cat = "plugin";
  if (!PyArg_ParseTuple(args, "s|s", &name, &cat)) {
    return nullptr;
  }
  PyObject* packed =
      Py_BuildValue("(ss)", name ? name : "", cat ? cat : "plugin");
  if (!packed) {
    return nullptr;
  }
  PyObject* r = debug_trace_event(self, packed);
  Py_DECREF(packed);
  return r;
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
    g_event_conns.push_back(HeldEvent(
        bus->subscribe<content::SelectionChanged>(
            [fn](const content::SelectionChanged&) {
              PyGILState_STATE gil = PyGILState_Ensure();
              PyObject* r = PyObject_CallFunction(fn, "");
              Py_XDECREF(r);
              if (PyErr_Occurred()) {
                PyErr_Print();
              }
              PyGILState_Release(gil);
            }),
        g_loading_plugin_dir));
  } else if (std::string(name) == "ExtentChanged") {
    g_event_conns.push_back(HeldEvent(
        bus->subscribe<content::ExtentChanged>(
            [fn](const content::ExtentChanged&) {
              PyGILState_STATE gil = PyGILState_Ensure();
              PyObject* r = PyObject_CallFunction(fn, "");
              Py_XDECREF(r);
              if (PyErr_Occurred()) {
                PyErr_Print();
              }
              PyGILState_Release(gil);
            }),
        g_loading_plugin_dir));
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

PyObject* ui_atmosphere_panel(PyObject*, PyObject*) {
  return wrap_new<ui::views::AtmospherePanel>();
}

PyObject* ui_debug_console(PyObject*, PyObject*) {
  return wrap_new<ui::views::DebugConsolePanel>();
}

PyObject* ui_measure_panel(PyObject*, PyObject*) {
  return wrap_new<ui::views::MeasurePanel>();
}

PyObject* ui_selection_panel(PyObject*, PyObject*) {
  return wrap_new<ui::views::SelectionPanel>();
}

PyObject* ui_show_inspect(PyObject*, PyObject* args) {
  const char* panel = nullptr;
  if (!PyArg_ParseTuple(args, "s", &panel)) {
    return nullptr;
  }
  plugin::ShellUiSink* ui = plugin::shell_ui(g_bound_host);
  if (!ui || !panel || !ui->show_inspect(panel)) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
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
    {"AtmospherePanel", ui_atmosphere_panel, METH_NOARGS, nullptr},
    {"DebugConsole", ui_debug_console, METH_NOARGS, nullptr},
    {"MeasurePanel", ui_measure_panel, METH_NOARGS, nullptr},
    {"SelectionPanel", ui_selection_panel, METH_NOARGS, nullptr},
    {"show_inspect", ui_show_inspect, METH_VARARGS,
     "show_inspect(panel) panel=measure|selection|legend|layer|atmosphere."},
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
    {"profile", debug_profile, METH_VARARGS,
     "profile(name, cat='plugin') — same as trace_event."},
    {"show_tab", debug_show_tab, METH_VARARGS,
     "show_tab(name) name=output|console|trace|memory."},
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

#endif  // HAS_PYTHON

}  // namespace

#if defined(HAS_PYTHON)
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

void set_python_load_directory(std::string_view directory) {
  g_loading_plugin_dir = std::string(directory);
}

void drop_python_plugin_holds(std::string_view directory) {
  drop_held_callables_for(directory);
}
#endif

void register_smartgis_bindings() {
#if defined(HAS_PYTHON)
  // Module is registered via PyImport_AppendInittab in PythonRuntime::init.
#endif
}

}  // namespace plugin
