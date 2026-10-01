// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/agent_ui.h"

#include <sstream>

#include "content/browser/debug/agent_json.h"

namespace content {
namespace detail {

bool dispatch_ui_method(const std::string& method,
                        const std::string& params_json,
                        int id,
                        const DebugAgentHost& host,
                        std::string* response) {
  if (!response) {
    return false;
  }
  if (method == "ui.find") {
    std::string name;
    extract_string_field(params_json, "name", &name);
    if (!host.ui_find) {
      *response = err_result(id, "ui host not bound");
      return true;
    }
    *response = ui_text_result(id, host.ui_find(name));
    return true;
  }
  if (method == "ui.click") {
    int x = 0;
    int y = 0;
    int button = 1;
    extract_int_field(params_json, "x", &x);
    extract_int_field(params_json, "y", &y);
    extract_int_field(params_json, "button", &button);
    if (button <= 0) {
      button = 1;
    }
    if (!host.ui_click) {
      *response = err_result(id, "ui host not bound");
      return true;
    }
    *response = ui_text_result(id, host.ui_click(x, y, button));
    return true;
  }
  if (method == "ui.type") {
    std::string text;
    extract_string_field(params_json, "text", &text);
    if (!host.ui_type) {
      *response = err_result(id, "ui host not bound");
      return true;
    }
    *response = ui_text_result(id, host.ui_type(text));
    return true;
  }
  if (method == "ui.dump_tree") {
    if (!host.ui_dump_tree) {
      *response = err_result(id, "ui host not bound");
      return true;
    }
    *response = ui_text_result(id, host.ui_dump_tree());
    return true;
  }
  if (method == "ui.overlay_stats") {
    if (!host.ui_overlay_stats) {
      *response = err_result(id, "ui host not bound");
      return true;
    }
    *response = ui_text_result(id, host.ui_overlay_stats());
    return true;
  }
  if (method == "ui.capture_shell") {
    std::string path;
    extract_string_field(params_json, "path", &path);
    if (!host.ui_capture_shell) {
      *response = err_result(id, "ui host not bound");
      return true;
    }
    *response = ui_text_result(id, host.ui_capture_shell(path));
    return true;
  }
  if (method == "script.run") {
    std::string path;
    extract_string_field(params_json, "path", &path);
    if (!host.script_run) {
      *response = err_result(id, "script host not bound");
      return true;
    }
    *response = ui_text_result(id, host.script_run(path));
    return true;
  }
  return false;
}

bool exec_ui_command(const std::string& line,
                     const DebugAgentHost& host,
                     std::string* output) {
  if (!output || line.rfind(":ui ", 0) != 0) {
    return false;
  }
  const std::string rest = line.substr(4);
  if (rest.rfind("find ", 0) == 0) {
    if (!host.ui_find) {
      *output = "ui host not bound";
      return true;
    }
    *output = host.ui_find(rest.substr(5));
    return true;
  }
  if (rest.rfind("click ", 0) == 0) {
    if (!host.ui_click) {
      *output = "ui host not bound";
      return true;
    }
    int x = 0;
    int y = 0;
    std::istringstream iss(rest.substr(6));
    if (!(iss >> x >> y)) {
      *output = "usage: :ui click <x> <y>";
      return true;
    }
    *output = host.ui_click(x, y, 1);
    return true;
  }
  if (rest.rfind("type ", 0) == 0) {
    if (!host.ui_type) {
      *output = "ui host not bound";
      return true;
    }
    *output = host.ui_type(rest.substr(5));
    return true;
  }
  if (rest == "tree") {
    if (!host.ui_dump_tree) {
      *output = "ui host not bound";
      return true;
    }
    *output = host.ui_dump_tree();
    return true;
  }
  if (rest == "overlay") {
    if (!host.ui_overlay_stats) {
      *output = "ui host not bound";
      return true;
    }
    *output = host.ui_overlay_stats();
    return true;
  }
  if (rest == "capture" || rest.rfind("capture ", 0) == 0) {
    if (!host.ui_capture_shell) {
      *output = "ui host not bound";
      return true;
    }
    std::string path;
    if (rest.size() > 8) {
      path = rest.substr(8);
    }
    *output = host.ui_capture_shell(path);
    return true;
  }
  *output = "unknown :ui command (see :help)";
  return true;
}

bool exec_script_command(const std::string& line,
                         const DebugAgentHost& host,
                         std::string* output) {
  if (!output || line.rfind(":script ", 0) != 0) {
    return false;
  }
  if (!host.script_run) {
    *output = "script host not bound";
    return true;
  }
  *output = host.script_run(line.substr(8));
  return true;
}

}  // namespace detail
}  // namespace content
