// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/feature_attrs.h"

#include <cstdio>

namespace content {

std::string encode_feature_token(const FeatureId& id) {
  std::string token = "fid:";
  for (uint8_t i = 0; i < id.len && i < sizeof(id.bytes); ++i) {
    char hex[3];
    std::snprintf(hex, sizeof(hex), "%02x",
                  static_cast<unsigned>(id.bytes[i]));
    token += hex;
  }
  return token;
}

FeatureId decode_feature_token(std::string_view token) {
  FeatureId id{};
  std::string_view hex = token;
  constexpr std::string_view kPrefix = "fid:";
  if (hex.size() >= kPrefix.size() && hex.substr(0, kPrefix.size()) == kPrefix) {
    hex.remove_prefix(kPrefix.size());
  }
  size_t n = 0;
  for (size_t i = 0; i + 1 < hex.size() && n < sizeof(id.bytes); i += 2) {
    unsigned v = 0;
    if (std::sscanf(hex.data() + i, "%2x", &v) != 1) {
      break;
    }
    id.bytes[n++] = static_cast<uint8_t>(v);
  }
  id.len = static_cast<uint8_t>(n);
  return id;
}

bool apply_named_field(std::vector<NamedField>* fields,
                       std::string_view field_name,
                       std::string_view value) {
  if (!fields || field_name.empty()) {
    return false;
  }
  for (NamedField& item : *fields) {
    if (item.name == field_name) {
      item.value.assign(value.data(), value.size());
      return true;
    }
  }
  fields->push_back(
      NamedField{std::string(field_name), std::string(value)});
  return true;
}

}  // namespace content
