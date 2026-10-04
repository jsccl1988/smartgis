// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/cmd/agent_sdbd.h"

#include <sstream>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "content/browser/debug/wire/agent_json.h"
#include "gis/datasource/sdbd/sdbd_client.h"

namespace content {
namespace detail {

bool dispatch_sdbd_method(const std::string& method,
                          const std::string& params_json,
                          int id,
                          std::string* response) {
  if (!response) {
    return false;
  }
  if (method != "sdbd.capabilities" && method != "sdbd.collections" &&
      method != "sdbd.query") {
    return false;
  }
  gis::datasource::SdbdClient client;
  gis::datasource::SdbdCallResult r;
  if (method == "sdbd.capabilities") {
    r = client.get_capabilities();
  } else if (method == "sdbd.collections") {
    r = client.get_collections();
  } else {
    std::string body;
    extract_string_field(params_json, "body", &body);
    r = client.post_query(body);
  }
  std::ostringstream oss;
  oss << "{\"ok\":" << (r.ok ? "true" : "false") << ",\"status\":" << r.status
      << ",\"body\":\"" << json_escape(r.body) << "\",\"error\":\""
      << json_escape(r.error) << "\"}";
  *response = ok_result(id, oss.str());
  return true;
}

bool exec_sdbd_command(const std::string& line, std::string* output) {
  if (!output) {
    return false;
  }
  if (line == ":sdbd capabilities") {
    gis::datasource::SdbdClient client;
    auto r = client.get_capabilities();
    *output = r.ok ? r.body : ("error: " + r.error);
    return true;
  }
  if (line.rfind(":sdbd sql ", 0) == 0) {
    const std::string sql = line.substr(10);
    gis::datasource::SdbdClient client;
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("sql");
    w.String(sql.c_str(), static_cast<rapidjson::SizeType>(sql.size()));
    w.EndObject();
    auto r = client.post_query(std::string(buf.GetString(), buf.GetSize()));
    *output = r.ok ? r.body : ("error: " + r.error);
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace content
