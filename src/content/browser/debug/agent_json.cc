// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/agent_json.h"

#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace content {
namespace detail {

std::string json_escape(const std::string& s) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.String(s.c_str(), static_cast<rapidjson::SizeType>(s.size()));
  const char* p = buf.GetString();
  const size_t n = buf.GetSize();
  if (n >= 2 && p[0] == '"' && p[n - 1] == '"') {
    return std::string(p + 1, n - 2);
  }
  return std::string(p, n);
}

bool extract_string_field(const std::string& json,
                          const char* key,
                          std::string* out) {
  if (!out) {
    return false;
  }
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }
  const auto it = doc.FindMember(key);
  if (it == doc.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return true;
}

bool extract_int_field(const std::string& json, const char* key, int* out) {
  if (!out) {
    return false;
  }
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }
  const auto it = doc.FindMember(key);
  if (it == doc.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetInt();
  return true;
}

std::string ok_result(int id, const std::string& result_obj) {
  std::ostringstream oss;
  oss << "{\"id\":" << id << ",\"ok\":true,\"result\":" << result_obj << "}";
  return oss.str();
}

std::string err_result(int id, const std::string& error) {
  std::ostringstream oss;
  oss << "{\"id\":" << id << ",\"ok\":false,\"error\":\"" << json_escape(error)
      << "\"}";
  return oss.str();
}

std::string ui_text_result(int id, const std::string& text) {
  return ok_result(id, std::string("{\"text\":\"") + json_escape(text) + "\"}");
}

}  // namespace detail
}  // namespace content
