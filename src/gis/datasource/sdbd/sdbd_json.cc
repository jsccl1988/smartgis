// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/sdbd/sdbd_json.h"

#include <cmath>

namespace gis {
namespace datasource {
namespace {

std::string id_from_value(const rapidjson::Value& value) {
  if (value.IsString()) {
    return std::string(value.GetString(), value.GetStringLength());
  }
  if (value.IsInt64()) {
    return std::to_string(value.GetInt64());
  }
  if (value.IsUint64()) {
    return std::to_string(value.GetUint64());
  }
  if (value.IsNumber()) {
    const double n = value.GetDouble();
    if (std::fabs(n - std::llround(n)) < 1e-9) {
      return std::to_string(static_cast<long long>(std::llround(n)));
    }
    return std::to_string(n);
  }
  return {};
}

}  // namespace

bool parse_json(const std::string& text,
                rapidjson::Document* out,
                std::string* err) {
  if (!out) {
    return false;
  }
  out->Parse(text.data(), static_cast<rapidjson::SizeType>(text.size()));
  if (out->HasParseError()) {
    if (err) {
      *err = "bad_request";
    }
    out->SetNull();
    return false;
  }
  return true;
}

std::vector<std::string> parse_collection_ids(const std::string& body) {
  std::vector<std::string> ids;
  rapidjson::Document root;
  std::string err;
  if (!parse_json(body, &root, &err) || !root.IsObject()) {
    return ids;
  }
  const auto it = root.FindMember("collections");
  if (it == root.MemberEnd() || !it->value.IsArray()) {
    return ids;
  }
  for (const auto& item : it->value.GetArray()) {
    if (item.IsObject()) {
      const auto id_it = item.FindMember("id");
      if (id_it == item.MemberEnd()) {
        continue;
      }
      const std::string id = id_from_value(id_it->value);
      if (!id.empty()) {
        ids.push_back(id);
      }
    } else if (item.IsString() && item.GetStringLength() > 0) {
      ids.emplace_back(item.GetString(), item.GetStringLength());
    }
  }
  return ids;
}

}  // namespace datasource
}  // namespace gis
