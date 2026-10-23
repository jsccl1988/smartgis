// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Installs Host / content / ui / debug / gis / script on glang.plugin.
// Surface / gis / script only export factories; this file is the sole runner.

#include "content/browser/glang.runtime/exec/py/embed/assemble.h"

#include "content/browser/glang.runtime/exec/py/embed/gis/gis.h"
#include "content/browser/glang.runtime/exec/py/embed/kernel/py_util.h"
#include "content/browser/glang.runtime/exec/py/embed/kernel/session.h"
#include "content/browser/glang.runtime/exec/py/embed/script/script.h"
#include "content/browser/glang.runtime/exec/py/embed/surface/surface.h"

#include "content/browser/glang.runtime/host/bind/console.h"

namespace plugin {
#if defined(HAS_PYTHON)

namespace {

bool attach_gis_module(PyObject* smartgis_module);

PyModuleDef kContentMod = {PyModuleDef_HEAD_INIT, "glang.plugin.content", nullptr,
                           -1, nullptr};
bool fill_glang_plugin(PyObject* module) {
  if (!module || !detail::init_host_type() || !detail::init_debug_type()) {
    return false;
  }
  PyObject* content = PyModule_Create(&kContentMod);
  PyObject* events = detail::make_events_module();
  PyObject* ui = detail::make_ui_module();
  PyObject* debug = detail::make_debug_module();
  auto drop_loose = [&]() {
    Py_XDECREF(debug);
    Py_XDECREF(ui);
    Py_XDECREF(events);
    Py_XDECREF(content);
  };
  if (!content || !events || !ui || !debug) {
    drop_loose();
    if (!PyErr_Occurred()) {
      PyErr_SetString(PyExc_RuntimeError, "glang.plugin submodules failed to init");
    }
    return false;
  }

  PyObject* host_type = reinterpret_cast<PyObject*>(detail::host_type());
  Py_INCREF(host_type);
  if (PyModule_AddObject(module, "Host", host_type) < 0) {
    Py_DECREF(host_type);
    drop_loose();
    return false;
  }
  if (!detail::add_module_object(content, "events", events, &events)) {
    drop_loose();
    return false;
  }
  Py_INCREF(Py_None);
  if (PyModule_AddObject(content, "host", Py_None) < 0) {
    Py_DECREF(Py_None);
    drop_loose();
    return false;
  }
  if (!detail::add_module_object(module, "content", content, &content)) {
    drop_loose();
    return false;
  }
  if (!detail::add_module_object(module, "ui", ui, &ui)) {
    drop_loose();
    return false;
  }
  if (!detail::add_module_object(module, "debug", debug, &debug)) {
    drop_loose();
    return false;
  }
  if (!attach_gis_module(module)) {
    if (!PyErr_Occurred()) {
      PyErr_SetString(PyExc_RuntimeError, "glang.plugin.gis failed to attach");
    }
    return false;
  }
  detail::attach_script_module(module);
  if (PyErr_Occurred()) {
    return false;
  }
  return true;
}

}  // namespace

bool attach_plugin_embed() {
  PyObject* module = PyImport_ImportModule("glang.plugin");
  if (!module) {
    PyErr_Print();
    return false;
  }
  const bool filled = fill_glang_plugin(module);
  if (!filled) {
    PyErr_Print();
    Py_DECREF(module);
    return false;
  }
  PyObject* modules = PyImport_GetModuleDict();
  if (modules && PyDict_SetItemString(modules, "smartgis", module) < 0) {
    PyErr_Print();
    Py_DECREF(module);
    return false;
  }
  Py_DECREF(module);
  return true;
}

PyObject* make_python_host(content::PluginHost* host) {
  return detail::make_host_object(host);
}

void bind_python_host(content::PluginHost* host) {
  detail::bound_host() = host;
  content::bind_console_plugin(host);
  PyObject* smartgis = PyImport_ImportModule("smartgis");
  if (!smartgis) {
    return;
  }
  PyObject* content_mod = PyObject_GetAttrString(smartgis, "content");
  PyObject* wrapper = detail::make_host_object(host);
  if (content_mod && wrapper) {
    PyObject_SetAttrString(content_mod, "host", wrapper);
  }
  Py_XDECREF(wrapper);
  Py_XDECREF(content_mod);
  Py_DECREF(smartgis);
}

void unbind_python_host() {
  detail::drop_all_holds();
  detail::bound_host() = nullptr;
  content::bind_console_plugin(nullptr);
}

void set_python_load_directory(std::string_view directory) {
  detail::loading_directory() = std::string(directory);
}

void drop_python_plugin_holds(std::string_view directory) {
  detail::drop_holds_for(directory);
}

namespace {

bool attach_gis_module(PyObject* smartgis_module) {
  if (!smartgis_module) {
    return false;
  }
  static PyModuleDef kGisMod = {
      PyModuleDef_HEAD_INIT, "glang.plugin.gis",
      "Bindings mirroring src/gis (analysis + scene + style).", -1, nullptr};
  PyObject* gis = PyModule_Create(&kGisMod);
  PyObject* analysis = detail::make_analysis_module();
  PyObject* scene = detail::make_scene_module();
  PyObject* style = detail::make_style_module();
  if (!gis || !analysis || !scene || !style) {
    Py_XDECREF(gis);
    Py_XDECREF(analysis);
    Py_XDECREF(scene);
    Py_XDECREF(style);
    return false;
  }
  if (!detail::add_module_object(gis, "analysis", analysis, &analysis)) {
    Py_DECREF(scene);
    Py_DECREF(style);
    Py_DECREF(gis);
    return false;
  }
  if (!detail::add_module_object(gis, "scene", scene, &scene)) {
    Py_DECREF(style);
    Py_DECREF(gis);
    return false;
  }
  if (!detail::add_module_object(gis, "style", style, &style)) {
    Py_DECREF(gis);
    return false;
  }
  return detail::add_module_object(smartgis_module, "gis", gis, &gis);
}

}  // namespace
#else
bool attach_plugin_embed() {
  return true;
}
#endif
}  // namespace plugin
