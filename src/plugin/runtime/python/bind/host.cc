// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/bind/forward.h"
#include "plugin/runtime/python/bind/gil.h"
#include "plugin/runtime/python/bind/py_compat.h"
#include "plugin/runtime/python/bind/slot.h"
#include "plugin/runtime/python/bind/surface.h"

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "tool/command/command.h"

#include <cstdint>
#include <string>
#include <utility>

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

using detail::HostObject;

bool retain_callback(PyObject* callable, std::uint64_t* id) {
  if (!id) {
    return false;
  }
  *id = detail::hold_callable(callable);
  if (*id == 0) {
    PyErr_SetString(PyExc_RuntimeError, "glang.plugin could not hold callback");
    return false;
  }
  return true;
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
  std::uint64_t callback_id = 0;
  if (!retain_callback(callable, &callback_id)) {
    return nullptr;
  }
  const bool ok = self->host->contribute_command(
      plugin_id, command_id, title, menu,
      [callback_id](const tool::CommandArgs& ca) {
        const std::string payload(ca.payload);
        return detail::call_py_bool(callback_id, payload.c_str());
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
  std::uint64_t callback_id = 0;
  if (!retain_callback(callable, &callback_id)) {
    return nullptr;
  }
  content::DialogContribution dlg;
  dlg.id = dialog_id ? dialog_id : "";
  dlg.title = title ? title : "";
  const bool ok = self->host->contribute_dialog(
      plugin_id, dlg,
      [callback_id](content::PluginHost*) { detail::call_py_void(callback_id); });
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
  std::uint64_t callback_id = 0;
  if (!retain_callback(callable, &callback_id)) {
    return nullptr;
  }
  content::DockContribution dock;
  dock.id = dock_id ? dock_id : "";
  dock.title = title ? title : "";
  dock.area = area ? area : "right";
  const bool ok = self->host->contribute_dock(
      plugin_id, dock,
      [callback_id](content::PluginHost*) { detail::call_py_void(callback_id); });
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
  std::uint64_t callback_id = 0;
  if (!retain_callback(callable, &callback_id)) {
    return nullptr;
  }
  content::ProcessingContribution proc;
  proc.id = proc_id ? proc_id : "";
  proc.title = title ? title : "";
  const bool ok = self->host->contribute_processing(
      plugin_id, proc,
      [callback_id](content::PluginHost*, std::string_view args_json) {
        const std::string json(args_json);
        return detail::call_py_bool(callback_id, json.c_str());
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
  bool ran = false;
  {
    // Kernels and contributed handlers run without the GIL. A Python
    // handler re-acquires it inside call_py_bool.
    detail::ReleaseGil released;
    ran = self->host->run_processing(processing_id, args_json ? args_json : "{}");
  }
  if (!ran) {
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
  bool ran = false;
  {
    detail::ReleaseGil released;
    ran = self->host->execute(command_id, ca);
  }
  if (!ran) {
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
  bool wrote = false;
  if (sink) {
    detail::ReleaseGil released;
    wrote = sink->export_bmp(path ? path : "", w, h);
  }
  if (!wrote) {
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

// PluginHost surface, not glang.plugin verbs. Rows stay as local wrappers.
// See bind/forward.h. Do not add PyMethodDef entries.
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

}  // namespace

namespace detail {

bool init_host_type() {
  host_type() = reinterpret_cast<PyTypeObject*>(PyType_FromSpec(&kHostSpec));
  return host_type() != nullptr;
}

PyObject* make_host_object(content::PluginHost* host) {
  if (!host_type()) {
    return nullptr;
  }
  PyObject* raw = PyObject_CallNoArgs(reinterpret_cast<PyObject*>(host_type()));
  if (!raw) {
    return nullptr;
  }
  reinterpret_cast<HostObject*>(raw)->host = host;
  return raw;
}

}  // namespace detail
#endif
}  // namespace plugin
