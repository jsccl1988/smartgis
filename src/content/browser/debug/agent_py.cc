// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/agent_py.h"

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/debug/agent_json.h"

namespace content {
namespace detail {

std::string run_python_code_oop(const std::string& code) {
  const char* py = std::getenv("SG_PYTHON");
  std::string exe = (py && py[0]) ? py : "python";
  const auto tmp = std::filesystem::temp_directory_path() /
                   ("sg_debug_py_" + std::to_string(GetCurrentProcessId()) +
                    ".py");
  {
    std::ofstream out(tmp, std::ios::binary);
    out << code;
  }
  std::string cmd = "\"" + exe + "\" \"" + tmp.string() + "\"";
  FILE* pipe = _popen(cmd.c_str(), "r");
  if (!pipe) {
    std::filesystem::remove(tmp);
    return std::string("error: failed to spawn python (") + exe + ")";
  }
  std::string output;
  char line[1024];
  while (std::fgets(line, sizeof(line), pipe)) {
    output += line;
  }
  const int rc = _pclose(pipe);
  std::filesystem::remove(tmp);
  if (rc != 0 && output.empty()) {
    return "error: python exited " + std::to_string(rc);
  }
  return output;
}

bool dispatch_py_method(const std::string& method,
                        const std::string& params_json,
                        int id,
                        const PyEvalFn& eval,
                        std::string* response) {
  if (!response || !eval) {
    return false;
  }
  if (method == "py.eval") {
    std::string code;
    extract_string_field(params_json, "code", &code);
    const std::string out = eval(code);
    *response =
        ok_result(id, std::string("{\"output\":\"") + json_escape(out) + "\"}");
    return true;
  }
  if (method == "py.run_file") {
    std::string path;
    extract_string_field(params_json, "path", &path);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      *response = err_result(id, "cannot read file");
      return true;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string out = eval(ss.str());
    *response =
        ok_result(id, std::string("{\"output\":\"") + json_escape(out) + "\"}");
    return true;
  }
  return false;
}

bool exec_py_command(const std::string& line,
                     const PyEvalFn& eval,
                     std::string* output) {
  if (!output || !eval) {
    return false;
  }
  if (line.rfind(":py ", 0) == 0) {
    *output = eval(line.substr(4));
    return true;
  }
  if (line.rfind(":run ", 0) == 0) {
    std::ifstream in(line.substr(5), std::ios::binary);
    if (!in) {
      *output = "cannot read file";
      return true;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    // Long :run stays OOP-capable via eval fallback when py_eval unset.
    *output = eval(ss.str());
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace content
