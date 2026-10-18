// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/bind/forward.h"
#include "plugin/runtime/python/bind/py_compat.h"
#include "plugin/runtime/python/bind/slot.h"
#include "plugin/runtime/python/bind/surface.h"

#include "content/browser/glang.runtime/host/bind/gis_console.h"
#include "plugin/runtime/python/gis/attach.h"
#include "plugin/runtime/python/script/attach.h"

#include <string>
#include <string_view>

namespace plugin {
#if defined(HAS_PYTHON)

namespace {

PyModuleDef kContentMod = {PyModuleDef_HEAD_INIT, "smartgis.content", nullptr, -1,
                           nullptr};
PyModuleDef kSmartgisMod = {PyModuleDef_HEAD_INIT, "smartgis", nullptr, -1,
                            nullptr};

PyObject* init_smartgis() {
  if (!detail::init_host_type() || !detail::init_debug_type()) {
    return nullptr;
  }
  PyObject* module = PyModule_Create(&kSmartgisMod);
  PyObject* content = PyModule_Create(&kContentMod);
  PyObject* events = detail::make_events_module();
  PyObject* tool = detail::make_tool_module();
  PyObject* ui = detail::make_ui_module();
  PyObject* debug = detail::make_debug_module();
  if (!module || !content || !events || !tool || !ui || !debug) {
    return nullptr;
  }
  Py_INCREF(reinterpret_cast<PyObject*>(detail::host_type()));
  PyModule_AddObject(module, "Host", reinterpret_cast<PyObject*>(detail::host_type()));
  PyModule_AddObject(content, "events", events);
  Py_INCREF(Py_None);
  PyModule_AddObject(content, "host", Py_None);
  PyModule_AddObject(module, "content", content);
  PyModule_AddObject(module, "tool", tool);
  PyModule_AddObject(module, "ui", ui);
  if (PyModule_AddObject(module, "debug", debug) < 0) {
    Py_DECREF(debug);
    return nullptr;
  }
  if (!attach_gis_module(module)) {
    return nullptr;
  }
  attach_script_module(module);
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return module;
}

}  // namespace

extern "C" PyObject* PyInit_smartgis() {
  return init_smartgis();
}

PyObject* make_python_host(content::PluginHost* host) {
  return detail::make_host_object(host);
}

void bind_python_host(content::PluginHost* host) {
  detail::bound_host() = host;
  content::bind_gis_host_for_analysis(host);
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
  content::bind_gis_host_for_analysis(nullptr);
}

void set_python_load_directory(std::string_view directory) {
  detail::loading_directory() = std::string(directory);
}

void drop_python_plugin_holds(std::string_view directory) {
  detail::drop_holds_for(directory);
}

void unregister_python_plugin_verbs(std::string_view directory) {
  detail::unregister_tracked_verbs(directory);
}

#endif

void register_smartgis_bindings() {}

}  // namespace plugin
