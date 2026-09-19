// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/style/expression.h"

#include <cstdlib>

#include "gis/style/json_mini.h"

namespace gis {
namespace style {
namespace {

using detail::JsonKind;
using detail::JsonValue;

bool is_expr_op(const std::string& op) {
  return op == "get" || op == "literal" || op == "zoom" || op == "==" ||
         op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=";
}

double expr_as_number(const ExprValue& v, bool* ok) { return v.as_number(ok); }

std::string expr_as_string(const ExprValue& v) { return v.as_string(); }

bool compare_expr(const std::string& op, const ExprValue& left,
                  const ExprValue& right) {
  bool lok = false;
  bool rok = false;
  const double ln = expr_as_number(left, &lok);
  const double rn = expr_as_number(right, &rok);
  if (lok && rok) {
    if (op == "==") {
      return ln == rn;
    }
    if (op == "!=") {
      return ln != rn;
    }
    if (op == "<") {
      return ln < rn;
    }
    if (op == "<=") {
      return ln <= rn;
    }
    if (op == ">") {
      return ln > rn;
    }
    if (op == ">=") {
      return ln >= rn;
    }
  }
  const std::string ls = expr_as_string(left);
  const std::string rs = expr_as_string(right);
  if (op == "==") {
    return ls == rs;
  }
  if (op == "!=") {
    return ls != rs;
  }
  if (op == "<") {
    return ls < rs;
  }
  if (op == "<=") {
    return ls <= rs;
  }
  if (op == ">") {
    return ls > rs;
  }
  if (op == ">=") {
    return ls >= rs;
  }
  return false;
}

bool eval_json(const JsonValue& v, const AttrMap& attrs, double zoom,
               ExprValue* out) {
  if (!out) {
    return false;
  }
  *out = ExprValue();

  switch (v.kind) {
    case JsonKind::kNull:
      out->kind = ExprValue::Kind::kNull;
      return true;
    case JsonKind::kBool:
      out->kind = ExprValue::Kind::kBool;
      out->b = v.b;
      return true;
    case JsonKind::kNumber:
      out->kind = ExprValue::Kind::kNumber;
      out->n = v.n;
      return true;
    case JsonKind::kString:
      out->kind = ExprValue::Kind::kString;
      out->s = v.s;
      return true;
    case JsonKind::kArray:
      break;
    default:
      return false;
  }

  if (v.a.empty() || !v.a[0].is_string() || !is_expr_op(v.a[0].s)) {
    return false;
  }

  const std::string& op = v.a[0].s;
  if (op == "get") {
    if (v.a.size() < 2 || !v.a[1].is_string()) {
      return false;
    }
    auto it = attrs.find(v.a[1].s);
    if (it == attrs.end()) {
      out->kind = ExprValue::Kind::kNull;
      return true;
    }
    out->kind = ExprValue::Kind::kString;
    out->s = it->second;
    return true;
  }
  if (op == "literal") {
    if (v.a.size() < 2) {
      return false;
    }
    // Payload is taken as-is (do not treat nested arrays as expressions).
    const JsonValue& payload = v.a[1];
    switch (payload.kind) {
      case JsonKind::kNull:
        out->kind = ExprValue::Kind::kNull;
        return true;
      case JsonKind::kBool:
        out->kind = ExprValue::Kind::kBool;
        out->b = payload.b;
        return true;
      case JsonKind::kNumber:
        out->kind = ExprValue::Kind::kNumber;
        out->n = payload.n;
        return true;
      case JsonKind::kString:
        out->kind = ExprValue::Kind::kString;
        out->s = payload.s;
        return true;
      default:
        out->kind = ExprValue::Kind::kString;
        out->s = detail::json_to_string(payload);
        return true;
    }
  }
  if (op == "zoom") {
    out->kind = ExprValue::Kind::kNumber;
    out->n = zoom;
    return true;
  }

  // Binary comparisons (reuse filter-style numeric/string rules).
  if (v.a.size() < 3) {
    return false;
  }
  ExprValue left;
  ExprValue right;
  if (!eval_json(v.a[1], attrs, zoom, &left) ||
      !eval_json(v.a[2], attrs, zoom, &right)) {
    return false;
  }
  out->kind = ExprValue::Kind::kBool;
  out->b = compare_expr(op, left, right);
  return true;
}

}  // namespace

bool looks_like_expression(const std::string& raw) {
  if (raw.empty() || raw[0] != '[') {
    return false;
  }
  JsonValue root;
  if (!detail::parse_json(raw.data(), raw.size(), &root) || !root.is_array() ||
      root.a.empty() || !root.a[0].is_string()) {
    return false;
  }
  return is_expr_op(root.a[0].s);
}

bool eval_expression(const std::string& json, const AttrMap& attrs, double zoom,
                     ExprValue* out) {
  if (!out || json.empty()) {
    return false;
  }
  JsonValue root;
  if (!detail::parse_json(json.data(), json.size(), &root)) {
    return false;
  }
  return eval_json(root, attrs, zoom, out);
}

}  // namespace style
}  // namespace gis
