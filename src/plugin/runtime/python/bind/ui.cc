// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/bind/forward.h"
#include "plugin/runtime/python/bind/py_compat.h"
#include "plugin/runtime/python/bind/slot.h"
#include "plugin/runtime/python/bind/surface.h"

#include "plugin/runtime/host/capability/capability.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/inspect/measure_panel.h"
#include "ui/gis/inspect/selection_panel.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

#include <string>
#include <typeinfo>
#include <utility>

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

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
  plugin::ShellUiSink* ui = plugin::shell_ui(detail::bound_host());
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

// Views widgets and dialogs, not glang.plugin verbs. See bind/forward.h.
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

PyModuleDef kUiMod = {PyModuleDef_HEAD_INIT, "smartgis.ui", nullptr, -1, kUiMethods};
PyModuleDef kConfigMod = {PyModuleDef_HEAD_INIT, "smartgis.ui.config",
                          "Shell ThemeService / system UI config.", -1,
                          kConfigMethods};

}  // namespace

namespace detail {

PyObject* make_ui_module() {
  PyObject* ui = PyModule_Create(&kUiMod);
  PyObject* config = PyModule_Create(&kConfigMod);
  if (!ui || !config) {
    Py_XDECREF(ui);
    Py_XDECREF(config);
    return nullptr;
  }
  if (PyModule_AddObject(ui, "config", config) < 0) {
    Py_DECREF(ui);
    return nullptr;
  }
  return ui;
}

}  // namespace detail
#endif
}  // namespace plugin
