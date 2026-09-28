// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/present/style/expression.h"

#include <cstdlib>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace gis {
namespace style {
namespace {

std::string write_json(const rapidjson::Value& v) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  v.Accept(writer);
  return std::string(buf.GetString(), buf.GetSize());
}

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

bool eval_json(const rapidjson::Value& v, const AttrMap& attrs, double zoom,
               ExprValue* out) {
  if (!out) {
    return false;
  }
  *out = ExprValue();

  if (v.IsNull()) {
    out->kind = ExprValue::Kind::kNull;
    return true;
  }
  if (v.IsBool()) {
    out->kind = ExprValue::Kind::kBool;
    out->b = v.GetBool();
    return true;
  }
  if (v.IsNumber()) {
    out->kind = ExprValue::Kind::kNumber;
    out->n = v.GetDouble();
    return true;
  }
  if (v.IsString()) {
    out->kind = ExprValue::Kind::kString;
    out->s = std::string(v.GetString(), v.GetStringLength());
    return true;
  }
  if (!v.IsArray()) {
    return false;
  }

  if (v.Empty() || !v[0].IsString()) {
    return false;
  }
  const std::string op(v[0].GetString(), v[0].GetStringLength());
  if (!is_expr_op(op)) {
    return false;
  }

  if (op == "get") {
    if (v.Size() < 2 || !v[1].IsString()) {
      return false;
    }
    const std::string key(v[1].GetString(), v[1].GetStringLength());
    auto it = attrs.find(key);
    if (it == attrs.end()) {
      out->kind = ExprValue::Kind::kNull;
      return true;
    }
    out->kind = ExprValue::Kind::kString;
    out->s = it->second;
    return true;
  }
  if (op == "literal") {
    if (v.Size() < 2) {
      return false;
    }
    // Payload is taken as-is (do not treat nested arrays as expressions).
    const rapidjson::Value& payload = v[1];
    if (payload.IsNull()) {
      out->kind = ExprValue::Kind::kNull;
      return true;
    }
    if (payload.IsBool()) {
      out->kind = ExprValue::Kind::kBool;
      out->b = payload.GetBool();
      return true;
    }
    if (payload.IsNumber()) {
      out->kind = ExprValue::Kind::kNumber;
      out->n = payload.GetDouble();
      return true;
    }
    if (payload.IsString()) {
      out->kind = ExprValue::Kind::kString;
      out->s = std::string(payload.GetString(), payload.GetStringLength());
      return true;
    }
    out->kind = ExprValue::Kind::kString;
    out->s = write_json(payload);
    return true;
  }
  if (op == "zoom") {
    out->kind = ExprValue::Kind::kNumber;
    out->n = zoom;
    return true;
  }

  // Binary comparisons (reuse filter-style numeric/string rules).
  if (v.Size() < 3) {
    return false;
  }
  ExprValue left;
  ExprValue right;
  if (!eval_json(v[1], attrs, zoom, &left) ||
      !eval_json(v[2], attrs, zoom, &right)) {
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
  rapidjson::Document root;
  root.Parse(raw.data(), static_cast<rapidjson::SizeType>(raw.size()));
  if (root.HasParseError() || !root.IsArray() || root.Empty() ||
      !root[0].IsString()) {
    return false;
  }
  return is_expr_op(
      std::string(root[0].GetString(), root[0].GetStringLength()));
}

bool eval_expression(const std::string& json, const AttrMap& attrs, double zoom,
                     ExprValue* out) {
  if (!out || json.empty()) {
    return false;
  }
  rapidjson::Document root;
  root.Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  if (root.HasParseError()) {
    return false;
  }
  return eval_json(root, attrs, zoom, out);
}

}  // namespace style
}  // namespace gis
