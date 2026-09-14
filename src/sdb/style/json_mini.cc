// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/style/json_mini.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

namespace sdb {
namespace style {
namespace detail {
namespace {

struct Parser {
  const char* cur = nullptr;
  const char* end = nullptr;

  void skip() {
    while (cur < end &&
           (*cur == ' ' || *cur == '\n' || *cur == '\r' || *cur == '\t')) {
      ++cur;
    }
  }

  bool eat(char c) {
    skip();
    if (cur < end && *cur == c) {
      ++cur;
      return true;
    }
    return false;
  }

  bool parse_string(std::string* out) {
    skip();
    if (!eat('"')) {
      return false;
    }
    out->clear();
    while (cur < end && *cur != '"') {
      if (*cur == '\\' && cur + 1 < end) {
        ++cur;
        switch (*cur) {
          case '"':
          case '\\':
          case '/':
            out->push_back(*cur);
            break;
          case 'n':
            out->push_back('\n');
            break;
          case 't':
            out->push_back('\t');
            break;
          case 'r':
            out->push_back('\r');
            break;
          default:
            out->push_back(*cur);
            break;
        }
        ++cur;
      } else {
        out->push_back(*cur++);
      }
    }
    return eat('"');
  }

  bool parse_number(double* out) {
    skip();
    if (cur >= end) {
      return false;
    }
    char* stop = nullptr;
    *out = std::strtod(cur, &stop);
    if (stop == cur) {
      return false;
    }
    cur = stop;
    return true;
  }

  bool parse_value(JsonValue* out) {
    skip();
    if (cur >= end) {
      return false;
    }
    if (*cur == '"') {
      out->kind = JsonKind::kString;
      return parse_string(&out->s);
    }
    if (*cur == '{') {
      return parse_object(out);
    }
    if (*cur == '[') {
      return parse_array(out);
    }
    if (cur + 4 <= end && std::strncmp(cur, "true", 4) == 0) {
      cur += 4;
      out->kind = JsonKind::kBool;
      out->b = true;
      return true;
    }
    if (cur + 5 <= end && std::strncmp(cur, "false", 5) == 0) {
      cur += 5;
      out->kind = JsonKind::kBool;
      out->b = false;
      return true;
    }
    if (cur + 4 <= end && std::strncmp(cur, "null", 4) == 0) {
      cur += 4;
      out->kind = JsonKind::kNull;
      return true;
    }
    out->kind = JsonKind::kNumber;
    return parse_number(&out->n);
  }

  bool parse_object(JsonValue* out) {
    if (!eat('{')) {
      return false;
    }
    out->kind = JsonKind::kObject;
    out->o.clear();
    skip();
    if (eat('}')) {
      return true;
    }
    for (;;) {
      std::string key;
      if (!parse_string(&key) || !eat(':')) {
        return false;
      }
      JsonValue child;
      if (!parse_value(&child)) {
        return false;
      }
      out->o.emplace(std::move(key), std::move(child));
      skip();
      if (eat('}')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_array(JsonValue* out) {
    if (!eat('[')) {
      return false;
    }
    out->kind = JsonKind::kArray;
    out->a.clear();
    skip();
    if (eat(']')) {
      return true;
    }
    for (;;) {
      JsonValue child;
      if (!parse_value(&child)) {
        return false;
      }
      out->a.push_back(std::move(child));
      skip();
      if (eat(']')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }
};

void write_escaped(std::ostringstream& os, const std::string& s) {
  os << '"';
  for (char c : s) {
    switch (c) {
      case '"':
        os << "\\\"";
        break;
      case '\\':
        os << "\\\\";
        break;
      case '\n':
        os << "\\n";
        break;
      case '\t':
        os << "\\t";
        break;
      case '\r':
        os << "\\r";
        break;
      default:
        os << c;
        break;
    }
  }
  os << '"';
}

}  // namespace

bool parse_json(const char* data, size_t len, JsonValue* out) {
  if (!data || !out || len == 0) {
    return false;
  }
  Parser p;
  p.cur = data;
  p.end = data + len;
  *out = JsonValue();
  if (!p.parse_value(out)) {
    return false;
  }
  p.skip();
  return p.cur == p.end;
}

std::string json_to_string(const JsonValue& v) {
  std::ostringstream os;
  switch (v.kind) {
    case JsonKind::kNull:
      os << "null";
      break;
    case JsonKind::kBool:
      os << (v.b ? "true" : "false");
      break;
    case JsonKind::kNumber:
      os << v.n;
      break;
    case JsonKind::kString:
      write_escaped(os, v.s);
      break;
    case JsonKind::kArray: {
      os << '[';
      for (size_t i = 0; i < v.a.size(); ++i) {
        if (i) {
          os << ',';
        }
        os << json_to_string(v.a[i]);
      }
      os << ']';
      break;
    }
    case JsonKind::kObject: {
      os << '{';
      bool first = true;
      for (const auto& kv : v.o) {
        if (!first) {
          os << ',';
        }
        first = false;
        write_escaped(os, kv.first);
        os << ':' << json_to_string(kv.second);
      }
      os << '}';
      break;
    }
  }
  return os.str();
}

}  // namespace detail
}  // namespace style
}  // namespace sdb
