// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_FEATURE_ATTRS_H_
#define GIS_FEATURE_ATTRS_H_

#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// POD feature identity / named-field helpers for edit sessions and host
// attribute tables. Header-only: no gis.dll import pragma so tool/gpu can
// include the POD subset without linking gis.

namespace gis {

// Opaque feature token (at most 32 bytes).
struct FeatureId {
  uint8_t bytes[32];
  uint8_t len;
};

// Name/value pair for AttributeTable / FeatureInfo string surfaces.
struct NamedField {
  std::string name;
  std::string value;
};

// Opaque token for FeatureId: "fid:" + lowercase hex of id.bytes[0..len).
inline std::string encode_feature_token(const FeatureId& id) {
  std::string token = "fid:";
  for (uint8_t i = 0; i < id.len && i < sizeof(id.bytes); ++i) {
    char hex[3];
    std::snprintf(hex, sizeof(hex), "%02x",
                  static_cast<unsigned>(id.bytes[i]));
    token += hex;
  }
  return token;
}

// Inverse of encode_feature_token. Also accepts a raw hex string without the
// "fid:" prefix. Returns a zero-length FeatureId when parsing yields no bytes.
inline FeatureId decode_feature_token(std::string_view token) {
  FeatureId id{};
  std::string_view hex = token;
  constexpr std::string_view kPrefix = "fid:";
  if (hex.size() >= kPrefix.size() &&
      hex.substr(0, kPrefix.size()) == kPrefix) {
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

// Update |fields| in place: overwrite the first matching |field_name|, or
// append. Returns false when |field_name| is empty.
inline bool apply_named_field(std::vector<NamedField>* fields,
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

}  // namespace gis

#endif  // GIS_FEATURE_ATTRS_H_
