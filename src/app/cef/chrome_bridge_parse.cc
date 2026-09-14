// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/chrome_bridge.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace app {
namespace cef {
namespace detail {
namespace {

void skip_ws(std::string_view s, size_t* i) {
  while (*i < s.size() &&
         (s[*i] == ' ' || s[*i] == '\t' || s[*i] == '\r' || s[*i] == '\n')) {
    ++(*i);
  }
}

bool parse_string(std::string_view s, size_t* i, std::string* out) {
  skip_ws(s, i);
  if (*i >= s.size() || s[*i] != '"') {
    return false;
  }
  ++(*i);
  out->clear();
  while (*i < s.size()) {
    const char c = s[*i];
    if (c == '"') {
      ++(*i);
      return true;
    }
    if (c == '\\' && *i + 1 < s.size()) {
      ++(*i);
      out->push_back(s[*i]);
      ++(*i);
      continue;
    }
    out->push_back(c);
    ++(*i);
  }
  return false;
}

bool find_key(std::string_view json, std::string_view key, size_t* value_pos) {
  const std::string needle = "\"" + std::string(key) + "\"";
  size_t pos = 0;
  while (true) {
    const size_t found = json.find(needle, pos);
    if (found == std::string_view::npos) {
      return false;
    }
    size_t i = found + needle.size();
    skip_ws(json, &i);
    if (i < json.size() && json[i] == ':') {
      ++i;
      skip_ws(json, &i);
      *value_pos = i;
      return true;
    }
    pos = found + 1;
  }
}

bool read_int_at(std::string_view json, size_t i, int* out) {
  skip_ws(json, &i);
  char* end = nullptr;
  const long v = std::strtol(json.data() + i, &end, 10);
  if (end == json.data() + i) {
    return false;
  }
  *out = static_cast<int>(v);
  return true;
}

bool read_uint_at(std::string_view json, size_t i, uint32_t* out) {
  int v = 0;
  if (!read_int_at(json, i, &v) || v < 0) {
    return false;
  }
  *out = static_cast<uint32_t>(v);
  return true;
}

bool read_float_at(std::string_view json, size_t i, float* out) {
  skip_ws(json, &i);
  char* end = nullptr;
  const float v = std::strtof(json.data() + i, &end);
  if (end == json.data() + i) {
    return false;
  }
  *out = v;
  return true;
}

bool read_string_field(std::string_view json,
                       std::string_view key,
                       std::string* out) {
  size_t i = 0;
  if (!find_key(json, key, &i)) {
    return false;
  }
  return parse_string(json, &i, out);
}

}  // namespace

// SG20-style topic envelopes map onto the existing BridgeType activate path.
BridgeType bridge_type_from_topic(std::string_view topic) {
  if (topic == "tool.command" || topic == "panel.action") {
    return BridgeType::kActivateTool;
  }
  return BridgeType::kUnknown;
}

BridgeType bridge_type_from_string(std::string_view type) {
  if (type == "ActivateTool") {
    return BridgeType::kActivateTool;
  }
  if (type == "CatalogOp") {
    return BridgeType::kCatalogOp;
  }
  if (type == "SelectMapTab") {
    return BridgeType::kSelectMapTab;
  }
  if (type == "OpenFile") {
    return BridgeType::kOpenFile;
  }
  if (type == "LayoutSlot") {
    return BridgeType::kLayoutSlot;
  }
  if (type == "QueryState") {
    return BridgeType::kQueryState;
  }
  if (type == "Exit") {
    return BridgeType::kExit;
  }
  if (type == "Ack") {
    return BridgeType::kAck;
  }
  if (type == "Error") {
    return BridgeType::kError;
  }
  if (type == "Status") {
    return BridgeType::kStatus;
  }
  if (type == "SelectionChanged") {
    return BridgeType::kSelectionChanged;
  }
  if (type == "ExtentChanged") {
    return BridgeType::kExtentChanged;
  }
  if (type == "LegendSnapshot") {
    return BridgeType::kLegendSnapshot;
  }
  if (type == "CatalogDelta") {
    return BridgeType::kCatalogDelta;
  }
  if (type == "ViewCursor") {
    return BridgeType::kViewCursor;
  }
  if (type == "Ready") {
    return BridgeType::kReady;
  }
  return BridgeType::kUnknown;
}

const char* bridge_type_to_string(BridgeType type) {
  switch (type) {
    case BridgeType::kActivateTool:
      return "ActivateTool";
    case BridgeType::kCatalogOp:
      return "CatalogOp";
    case BridgeType::kSelectMapTab:
      return "SelectMapTab";
    case BridgeType::kOpenFile:
      return "OpenFile";
    case BridgeType::kLayoutSlot:
      return "LayoutSlot";
    case BridgeType::kQueryState:
      return "QueryState";
    case BridgeType::kExit:
      return "Exit";
    case BridgeType::kAck:
      return "Ack";
    case BridgeType::kError:
      return "Error";
    case BridgeType::kStatus:
      return "Status";
    case BridgeType::kSelectionChanged:
      return "SelectionChanged";
    case BridgeType::kExtentChanged:
      return "ExtentChanged";
    case BridgeType::kLegendSnapshot:
      return "LegendSnapshot";
    case BridgeType::kCatalogDelta:
      return "CatalogDelta";
    case BridgeType::kViewCursor:
      return "ViewCursor";
    case BridgeType::kReady:
      return "Ready";
    default:
      return "Unknown";
  }
}

bool is_supported_api_version(int version) {
  return version == 1;
}

bool parse_bridge_message(std::string_view json, BridgeMessage* out) {
  if (!out) {
    return false;
  }
  *out = BridgeMessage{};
  size_t i = 0;
  const bool has_version =
      find_key(json, "api_version", &i) && read_int_at(json, i, &out->api_version);

  std::string type;
  const bool has_type = read_string_field(json, "type", &type);
  std::string topic;
  const bool has_topic = read_string_field(json, "topic", &topic);

  if (has_type) {
    if (!has_version) {
      return false;
    }
    out->type = bridge_type_from_string(type);
  } else if (has_topic) {
    out->type = bridge_type_from_topic(topic);
    if (out->type == BridgeType::kUnknown) {
      return false;
    }
    if (!has_version) {
      out->api_version = 1;
    }
  } else {
    return false;
  }

  read_string_field(json, "request_id", &out->request_id);
  if (find_key(json, "view_id", &i)) {
    read_uint_at(json, i, &out->view_id);
  }
  read_string_field(json, "command_id", &out->command_id);
  if (out->command_id.empty()) {
    read_string_field(json, "command", &out->command_id);
  }
  read_string_field(json, "op", &out->op_json);
  if (out->op_json.empty()) {
    read_string_field(json, "op_json", &out->op_json);
  }
  if (find_key(json, "index", &i) || find_key(json, "tab_index", &i)) {
    read_int_at(json, i, &out->tab_index);
  }
  read_string_field(json, "path", &out->path);
  read_string_field(json, "slot_id", &out->slot_id);
  read_string_field(json, "what", &out->query_what);
  read_string_field(json, "text", &out->text);
  if (find_key(json, "code", &i)) {
    read_int_at(json, i, &out->error_code);
  }
  if (find_key(json, "dpi", &i)) {
    read_float_at(json, i, &out->dpi);
  }
  int x = 0, y = 0, w = 0, h = 0;
  if (find_key(json, "x", &i)) {
    read_int_at(json, i, &x);
  }
  if (find_key(json, "y", &i)) {
    read_int_at(json, i, &y);
  }
  if (find_key(json, "w", &i)) {
    read_int_at(json, i, &w);
  }
  if (find_key(json, "h", &i)) {
    read_int_at(json, i, &h);
  }
  out->slot_rect = {x, y, w, h};
  return true;
}

std::string serialize_bridge_message(const BridgeMessage& msg) {
  std::ostringstream oss;
  oss << "{\"api_version\":" << msg.api_version << ",\"type\":\""
      << bridge_type_to_string(msg.type) << "\"";
  if (!msg.request_id.empty()) {
    oss << ",\"request_id\":\"" << msg.request_id << "\"";
  }
  oss << ",\"view_id\":" << msg.view_id;
  if (!msg.command_id.empty()) {
    oss << ",\"command_id\":\"" << msg.command_id << "\"";
  }
  if (!msg.text.empty()) {
    oss << ",\"text\":\"" << msg.text << "\"";
  }
  if (msg.error_code != 0) {
    oss << ",\"code\":" << msg.error_code;
  }
  if (!msg.op_json.empty()) {
    oss << ",\"op\":\"" << msg.op_json << "\"";
  }
  if (msg.type == BridgeType::kSelectMapTab) {
    oss << ",\"index\":" << msg.tab_index;
  }
  if (msg.type == BridgeType::kLayoutSlot) {
    oss << ",\"x\":" << msg.slot_rect.x << ",\"y\":" << msg.slot_rect.y
        << ",\"w\":" << msg.slot_rect.w << ",\"h\":" << msg.slot_rect.h
        << ",\"dpi\":" << msg.dpi;
  }
  if (!msg.query_what.empty()) {
    oss << ",\"what\":\"" << msg.query_what << "\"";
  }
  oss << "}";
  return oss.str();
}

}  // namespace detail
}  // namespace cef
}  // namespace app
