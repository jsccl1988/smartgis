// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_STYLE_JSON_MINI_H_
#define SDB_STYLE_JSON_MINI_H_

#include <map>
#include <string>
#include <vector>

namespace sdb {
namespace style {
namespace detail {

enum class JsonKind {
  kNull,
  kBool,
  kNumber,
  kString,
  kArray,
  kObject,
};

// Minimal JSON value tree for Style / symbol manifest parsing.
struct JsonValue {
  JsonKind kind = JsonKind::kNull;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<JsonValue> a;
  std::map<std::string, JsonValue> o;

  bool is_object() const { return kind == JsonKind::kObject; }
  bool is_array() const { return kind == JsonKind::kArray; }
  bool is_string() const { return kind == JsonKind::kString; }
  bool is_number() const { return kind == JsonKind::kNumber; }
  bool is_bool() const { return kind == JsonKind::kBool; }

  const JsonValue* get(const char* key) const {
    if (!is_object()) {
      return nullptr;
    }
    auto it = o.find(key);
    return it == o.end() ? nullptr : &it->second;
  }
};

bool parse_json(const char* data, size_t len, JsonValue* out);
std::string json_to_string(const JsonValue& v);

}  // namespace detail
}  // namespace style
}  // namespace sdb

#endif  // SDB_STYLE_JSON_MINI_H_
