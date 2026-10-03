// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/agent_record.h"

#include <rapidjson/document.h>

#include "content/browser/debug/agent_json.h"

namespace content {
namespace detail {

bool dispatch_record_method(const std::string& method,
                            const std::string& params_json,
                            int id,
                            const RecordHandlers& handlers,
                            std::string* response) {
  if (!response) {
    return false;
  }
  if (method == "record.enable") {
    bool on = true;
    {
      rapidjson::Document doc;
      if (!doc.Parse(params_json.c_str()).HasParseError() && doc.IsObject()) {
        const auto it = doc.FindMember("on");
        if (it != doc.MemberEnd() && it->value.IsBool()) {
          on = it->value.GetBool();
        }
      }
    }
    if (handlers.set_enabled) {
      handlers.set_enabled(on);
    }
    *response = ok_result(id, on ? "{\"on\":true}" : "{\"on\":false}");
    return true;
  }
  if (method == "record.poll") {
    const std::string events =
        handlers.poll_json ? handlers.poll_json() : std::string("[]");
    *response = ok_result(id, std::string("{\"events\":") + events + "}");
    return true;
  }
  if (method == "record.clear") {
    if (handlers.clear) {
      handlers.clear();
    }
    *response = ok_result(id, "{}");
    return true;
  }
  return false;
}

bool exec_record_command(const std::string& line,
                         const RecordHandlers& handlers,
                         std::string* output) {
  if (!output || line.rfind(":record", 0) != 0) {
    return false;
  }
  std::string rest = line.size() > 7 ? line.substr(7) : std::string();
  while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
    rest.erase(rest.begin());
  }
  if (rest == "on" || rest == "enable") {
    if (handlers.set_enabled) {
      handlers.set_enabled(true);
    }
    *output = "record on";
    return true;
  }
  if (rest == "off" || rest == "disable") {
    if (handlers.set_enabled) {
      handlers.set_enabled(false);
    }
    *output = "record off";
    return true;
  }
  if (rest == "poll") {
    *output = handlers.poll_json ? handlers.poll_json() : "[]";
    return true;
  }
  if (rest == "clear") {
    if (handlers.clear) {
      handlers.clear();
    }
    *output = "record cleared";
    return true;
  }
  *output = "usage: :record on|off|poll|clear";
  return true;
}

}  // namespace detail
}  // namespace content
