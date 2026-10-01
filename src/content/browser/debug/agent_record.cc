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

}  // namespace detail
}  // namespace content
