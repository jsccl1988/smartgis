// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/types.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace content {

std::string json_escape_string(std::string_view text) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.String(text.data(), static_cast<rapidjson::SizeType>(text.size()));
  // Writer emits a quoted JSON string; strip quotes for embed callers.
  const char* s = buf.GetString();
  const size_t n = buf.GetSize();
  if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
    return std::string(s + 1, n - 2);
  }
  return std::string(s, n);
}

std::string layers_to_catalog_json(const std::vector<LayerDesc>& layers) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartArray();
  for (const LayerDesc& layer : layers) {
    w.StartObject();
    w.Key("id");
    w.String(layer.id.c_str(),
             static_cast<rapidjson::SizeType>(layer.id.size()));
    w.Key("name");
    w.String(layer.name.c_str(),
             static_cast<rapidjson::SizeType>(layer.name.size()));
    w.Key("visible");
    w.Bool(layer.visible);
    w.EndObject();
  }
  w.EndArray();
  return std::string(buf.GetString(), buf.GetSize());
}

}  // namespace content
