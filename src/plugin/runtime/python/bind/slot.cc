// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/bind/slot.h"

#include "content/browser/glang.runtime/exec/session/plugin_fn.h"
#include "plugin/runtime/python/bind/forward.h"

#include <string>
#include <utility>
#include <vector>

namespace plugin {
namespace detail {
namespace {

content::PluginHost* g_bound_host = nullptr;
std::string g_loading_directory;

#if defined(HAS_PYTHON)

struct HeldId {
  std::uint64_t id = 0;
  std::string dir;
};

struct HeldEvent {
  content::EventBus::Connection conn;
  std::string dir;
  HeldEvent(content::EventBus::Connection c, std::string d)
      : conn(std::move(c)), dir(std::move(d)) {}
};

struct TrackedVerb {
  std::string name;
  std::string dir;
};

std::vector<HeldId> g_held_ids;
std::vector<HeldEvent> g_event_conns;
std::vector<TrackedVerb> g_tracked_verbs;
std::uint64_t g_next_callback_id = 1;
PyTypeObject* g_host_type = nullptr;

// Borrowed dict owned by the glang.plugin module. Null when the module is
// not installed. Caller holds the GIL.
PyObject* callback_holds() {
  PyObject* mod = PyImport_ImportModule("glang.plugin");
  if (!mod) {
    PyErr_Clear();
    return nullptr;
  }
  PyObject* dict = PyModule_GetDict(mod);
  PyObject* holds = dict ? PyDict_GetItemString(dict, "_callback_holds") : nullptr;
  if (!holds && dict) {
    holds = PyDict_New();
    if (!holds || PyDict_SetItemString(dict, "_callback_holds", holds) < 0) {
      Py_XDECREF(holds);
      Py_DECREF(mod);
      PyErr_Clear();
      return nullptr;
    }
    Py_DECREF(holds);
    holds = PyDict_GetItemString(dict, "_callback_holds");
  }
  Py_DECREF(mod);
  return holds;
}

void release_callback_id(std::uint64_t id) {
  if (id == 0) {
    return;
  }
  PyObject* holds = callback_holds();
  if (!holds) {
    return;
  }
  const std::string key = std::to_string(id);
  if (PyDict_DelItemString(holds, key.c_str()) < 0) {
    PyErr_Clear();
  }
}

PyObject* callback_object(std::uint64_t id) {
  PyObject* holds = callback_holds();
  if (!holds || id == 0) {
    return nullptr;
  }
  const std::string key = std::to_string(id);
  return PyDict_GetItemString(holds, key.c_str());
}

#endif

}  // namespace

content::PluginHost*& bound_host() {
  return g_bound_host;
}

std::string& loading_directory() {
  return g_loading_directory;
}

#if defined(HAS_PYTHON)

std::uint64_t hold_callable(PyObject* obj) {
  if (!obj) {
    return 0;
  }
  PyObject* holds = callback_holds();
  if (!holds) {
    return 0;
  }
  const std::uint64_t id = g_next_callback_id++;
  const std::string key = std::to_string(id);
  if (PyDict_SetItemString(holds, key.c_str(), obj) < 0) {
    PyErr_Clear();
    return 0;
  }
  g_held_ids.push_back(HeldId{id, g_loading_directory});
  return id;
}

void drop_all_holds() {
  for (const HeldId& held : g_held_ids) {
    release_callback_id(held.id);
  }
  g_held_ids.clear();
  g_event_conns.clear();
}

void drop_holds_for(std::string_view directory) {
  const std::string dir(directory);
  auto it = g_held_ids.begin();
  while (it != g_held_ids.end()) {
    if (it->dir == dir) {
      release_callback_id(it->id);
      it = g_held_ids.erase(it);
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

void hold_event(content::EventBus::Connection conn) {
  g_event_conns.push_back(HeldEvent(std::move(conn), g_loading_directory));
}

bool call_py_bool(std::uint64_t id, const char* arg_or_null) {
  PyGILState_STATE gil = PyGILState_Ensure();
  PyObject* callable = callback_object(id);
  bool pass = false;
  if (callable) {
    PyObject* result = arg_or_null ? PyObject_CallFunction(callable, "s", arg_or_null)
                                   : PyObject_CallFunction(callable, nullptr);
    if (!result) {
      PyErr_Print();
      pass = false;
    } else {
      pass = PyObject_IsTrue(result) != 0;
      Py_DECREF(result);
    }
  }
  PyGILState_Release(gil);
  return pass;
}

void call_py_void(std::uint64_t id) {
  PyGILState_STATE gil = PyGILState_Ensure();
  PyObject* callable = callback_object(id);
  if (callable) {
    PyObject* result = PyObject_CallFunction(callable, nullptr);
    if (!result) {
      PyErr_Print();
    } else {
      Py_DECREF(result);
    }
  }
  PyGILState_Release(gil);
}

PyTypeObject*& host_type() {
  return g_host_type;
}

#endif

void track_registered_verb(std::string_view name) {
#if defined(HAS_PYTHON)
  if (name.empty()) {
    return;
  }
  g_tracked_verbs.push_back(TrackedVerb{std::string(name), g_loading_directory});
#else
  (void)name;
#endif
}

void unregister_tracked_verbs(std::string_view directory) {
#if defined(HAS_PYTHON)
  const std::string dir(directory);
  auto it = g_tracked_verbs.begin();
  while (it != g_tracked_verbs.end()) {
    if (it->dir == dir) {
      content::py::unregister_fn(it->name);
      it = g_tracked_verbs.erase(it);
    } else {
      ++it;
    }
  }
#else
  (void)directory;
#endif
}

}  // namespace detail
}  // namespace plugin
