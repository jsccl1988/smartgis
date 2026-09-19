// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_FEATURE_ATTRS_H_
#define CONTENT_PUBLIC_FEATURE_ATTRS_H_

#include <string>
#include <string_view>
#include <vector>

#include "content/public/map_types.h"

namespace content {

// Name/value pair used by AttributeTable / FeatureInfo string surfaces.
// Chrome must not hold leftover GIS feature pointers.
struct NamedField {
  std::string name;
  std::string value;
};

// Opaque token for FeatureId: "fid:" + lowercase hex of id.bytes[0..len).
std::string encode_feature_token(const FeatureId& id);

// Inverse of encode_feature_token. Also accepts a raw hex string without the
// "fid:" prefix. Returns a zero-length FeatureId when parsing yields no bytes.
FeatureId decode_feature_token(std::string_view token);

// Update |fields| in place: overwrite the first matching |field_name|, or
// append. Returns false when |field_name| is empty.
bool apply_named_field(std::vector<NamedField>* fields,
                       std::string_view field_name,
                       std::string_view value);

}  // namespace content

#endif  // CONTENT_PUBLIC_FEATURE_ATTRS_H_
