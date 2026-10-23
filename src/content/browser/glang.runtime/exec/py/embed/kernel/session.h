// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Interpreter session shared by Python surfaces: bound PluginHost and the
// plugin directory being loaded. Callables live on glang.plugin; this side
// keeps only integer ids until stop.

#ifndef PLUGIN_RUNTIME_PYTHON_KERNEL_SESSION_H_
#define PLUGIN_RUNTIME_PYTHON_KERNEL_SESSION_H_

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"

#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace plugin {
namespace detail {

content::PluginHost*& bound_host();
std::string& loading_directory();

#if defined(HAS_PYTHON)

// Stores |obj| on glang.plugin and returns an id. 0 means the module could
// not take the reference. Does not keep |obj| in this process's plugin vector.
std::uint64_t hold_callable(PyObject* obj);
void drop_all_holds();
void drop_holds_for(std::string_view directory);
void hold_event(content::EventBus::Connection conn);

bool call_py_bool(std::uint64_t id, const char* arg_or_null);
void call_py_void(std::uint64_t id);

// Python wrapper around one PluginHost*. The type object lives with the host
// methods; this is only the instance layout.
struct HostObject {
  PyObject_HEAD
  content::PluginHost* host;
};

PyTypeObject*& host_type();

#endif

}  // namespace detail

#if defined(HAS_PYTHON)
void bind_python_host(content::PluginHost* host);
void unbind_python_host();
void set_python_load_directory(std::string_view directory);
void drop_python_plugin_holds(std::string_view directory);
#endif

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_KERNEL_SESSION_H_
