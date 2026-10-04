// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/carto/style/expression.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

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
         op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=" ||
         op == "interpolate" || op == "step" || op == "match" ||
         op == "case" || op == "coalesce";
}

double expr_as_number(const ExprValue& v, bool* ok) { return v.as_number(ok); }

std::string expr_as_string(const ExprValue& v) { return v.as_string(); }

bool values_equal(const ExprValue& left, const ExprValue& right) {
  bool lok = false;
  bool rok = false;
  const double ln = expr_as_number(left, &lok);
  const double rn = expr_as_number(right, &rok);
  if (lok && rok) {
    return ln == rn;
  }
  return expr_as_string(left) == expr_as_string(right);
}

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

int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

bool parse_color_argb(const std::string& text, uint32_t* out) {
  if (!out || text.empty()) {
    return false;
  }
  if (text[0] == '#') {
    const char* p = text.c_str() + 1;
    const size_t n = text.size() - 1;
    auto read = [&](size_t i) -> int { return hex_nibble(p[i]); };
    if (n == 3) {
      const int r = read(0);
      const int g = read(1);
      const int b = read(2);
      if (r < 0 || g < 0 || b < 0) {
        return false;
      }
      *out = 0xFF000000u | (static_cast<uint32_t>(r * 17) << 16) |
             (static_cast<uint32_t>(g * 17) << 8) |
             static_cast<uint32_t>(b * 17);
      return true;
    }
    if (n == 6) {
      const int r = (read(0) << 4) | read(1);
      const int g = (read(2) << 4) | read(3);
      const int b = (read(4) << 4) | read(5);
      if (r < 0 || g < 0 || b < 0) {
        return false;
      }
      *out = 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
             (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
      return true;
    }
    if (n == 8) {
      const int a = (read(0) << 4) | read(1);
      const int r = (read(2) << 4) | read(3);
      const int g = (read(4) << 4) | read(5);
      const int b = (read(6) << 4) | read(7);
      if (a < 0 || r < 0 || g < 0 || b < 0) {
        return false;
      }
      *out = (static_cast<uint32_t>(a) << 24) |
             (static_cast<uint32_t>(r) << 16) |
             (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
      return true;
    }
    return false;
  }
  if (text.size() >= 10 && text.compare(0, 4, "rgb(") == 0 &&
      text.back() == ')') {
    int r = 0;
    int g = 0;
    int b = 0;
    if (std::sscanf(text.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) != 3 &&
        std::sscanf(text.c_str(), "rgb(%d, %d, %d)", &r, &g, &b) != 3) {
      return false;
    }
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
      return false;
    }
    *out = 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    return true;
  }
  return false;
}

std::string format_color_argb(uint32_t argb) {
  char buf[16];
  const unsigned a = (argb >> 24) & 0xFFu;
  const unsigned r = (argb >> 16) & 0xFFu;
  const unsigned g = (argb >> 8) & 0xFFu;
  const unsigned b = argb & 0xFFu;
  if (a == 0xFFu) {
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  } else {
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", a, r, g, b);
  }
  return std::string(buf);
}

uint32_t lerp_argb(uint32_t a, uint32_t b, double t) {
  t = std::clamp(t, 0.0, 1.0);
  auto channel = [t](uint32_t x, uint32_t y, int shift) -> uint32_t {
    const double xa = static_cast<double>((x >> shift) & 0xFFu);
    const double ya = static_cast<double>((y >> shift) & 0xFFu);
    const double v = xa + (ya - xa) * t;
    return static_cast<uint32_t>(std::lround(v)) & 0xFFu;
  };
  return (channel(a, b, 24) << 24) | (channel(a, b, 16) << 16) |
         (channel(a, b, 8) << 8) | channel(a, b, 0);
}

// MapLibre-style progress for exponential interpolation between two stops.
double interpolate_progress(double t, bool exponential, double base) {
  t = std::clamp(t, 0.0, 1.0);
  if (!exponential || std::fabs(base - 1.0) < 1e-12) {
    return t;
  }
  return (std::pow(base, t) - 1.0) / (base - 1.0);
}

bool try_color(const ExprValue& v, uint32_t* out) {
  if (!out) {
    return false;
  }
  if (v.kind == ExprValue::Kind::kString) {
    return parse_color_argb(v.s, out);
  }
  return false;
}

bool lerp_outputs(const ExprValue& a, const ExprValue& b, double progress,
                  ExprValue* out) {
  if (!out) {
    return false;
  }
  uint32_t ca = 0;
  uint32_t cb = 0;
  if (try_color(a, &ca) && try_color(b, &cb)) {
    out->kind = ExprValue::Kind::kString;
    out->s = format_color_argb(lerp_argb(ca, cb, progress));
    return true;
  }
  bool aok = false;
  bool bok = false;
  const double an = expr_as_number(a, &aok);
  const double bn = expr_as_number(b, &bok);
  if (aok && bok) {
    out->kind = ExprValue::Kind::kNumber;
    out->n = an + (bn - an) * progress;
    return true;
  }
  // Non-numeric / non-color: snap to nearest stop (no string lerp).
  *out = progress < 0.5 ? a : b;
  return true;
}

bool eval_json(const rapidjson::Value& v, const AttrMap& attrs, double zoom,
               ExprValue* out);

bool match_label(const ExprValue& input, const rapidjson::Value& label,
                 const AttrMap& attrs, double zoom) {
  if (label.IsArray()) {
    // Label list: any element matches (MapLibre match label arrays).
    for (rapidjson::SizeType i = 0; i < label.Size(); ++i) {
      ExprValue one;
      if (!eval_json(label[i], attrs, zoom, &one)) {
        return false;
      }
      if (values_equal(input, one)) {
        return true;
      }
    }
    return false;
  }
  ExprValue one;
  if (!eval_json(label, attrs, zoom, &one)) {
    return false;
  }
  return values_equal(input, one);
}

bool eval_interpolate(const rapidjson::Value& v, const AttrMap& attrs,
                      double zoom, ExprValue* out) {
  // ["interpolate", ["linear"|"exponential", base?], input, stop, out, ...]
  if (v.Size() < 5 || (v.Size() % 2) == 0) {
    return false;
  }
  if (!v[1].IsArray() || v[1].Empty() || !v[1][0].IsString()) {
    return false;
  }
  const std::string kind(v[1][0].GetString(), v[1][0].GetStringLength());
  bool exponential = false;
  double base = 1.0;
  if (kind == "linear") {
    exponential = false;
  } else if (kind == "exponential") {
    exponential = true;
    base = 1.0;
    if (v[1].Size() >= 2 && v[1][1].IsNumber()) {
      base = v[1][1].GetDouble();
    }
  } else {
    // cubic-bezier and others: not in mini subset.
    return false;
  }

  ExprValue input;
  if (!eval_json(v[2], attrs, zoom, &input)) {
    return false;
  }
  bool input_ok = false;
  const double x = expr_as_number(input, &input_ok);
  if (!input_ok) {
    return false;
  }

  struct Stop {
    double at = 0;
    ExprValue value;
  };
  std::vector<Stop> stops;
  stops.reserve((v.Size() - 3) / 2);
  for (rapidjson::SizeType i = 3; i + 1 < v.Size(); i += 2) {
    if (!v[i].IsNumber()) {
      return false;
    }
    Stop s;
    s.at = v[i].GetDouble();
    if (!eval_json(v[i + 1], attrs, zoom, &s.value)) {
      return false;
    }
    stops.push_back(std::move(s));
  }
  if (stops.size() < 2) {
    return false;
  }

  if (x <= stops.front().at) {
    *out = stops.front().value;
    return true;
  }
  if (x >= stops.back().at) {
    *out = stops.back().value;
    return true;
  }
  for (size_t i = 0; i + 1 < stops.size(); ++i) {
    const Stop& a = stops[i];
    const Stop& b = stops[i + 1];
    if (x >= a.at && x <= b.at) {
      const double span = b.at - a.at;
      const double t = span == 0.0 ? 0.0 : (x - a.at) / span;
      const double progress = interpolate_progress(t, exponential, base);
      return lerp_outputs(a.value, b.value, progress, out);
    }
  }
  *out = stops.back().value;
  return true;
}

bool eval_step(const rapidjson::Value& v, const AttrMap& attrs, double zoom,
               ExprValue* out) {
  // ["step", input, output0, stop1, output1, stop2, output2, ...]
  if (v.Size() < 3 || (v.Size() % 2) == 0) {
    return false;
  }
  ExprValue input;
  if (!eval_json(v[1], attrs, zoom, &input)) {
    return false;
  }
  bool input_ok = false;
  const double x = expr_as_number(input, &input_ok);
  if (!input_ok) {
    return false;
  }
  ExprValue cur;
  if (!eval_json(v[2], attrs, zoom, &cur)) {
    return false;
  }
  for (rapidjson::SizeType i = 3; i + 1 < v.Size(); i += 2) {
    if (!v[i].IsNumber()) {
      return false;
    }
    const double stop = v[i].GetDouble();
    if (x >= stop) {
      if (!eval_json(v[i + 1], attrs, zoom, &cur)) {
        return false;
      }
    } else {
      break;
    }
  }
  *out = cur;
  return true;
}

bool eval_match(const rapidjson::Value& v, const AttrMap& attrs, double zoom,
                ExprValue* out) {
  // ["match", input, label0, output0, ..., default]
  // Labels may be scalars or arrays of scalars. Total length is odd (≥5).
  if (v.Size() < 5 || (v.Size() % 2) == 0) {
    return false;
  }
  ExprValue input;
  if (!eval_json(v[1], attrs, zoom, &input)) {
    return false;
  }
  const rapidjson::SizeType last = v.Size() - 1;
  for (rapidjson::SizeType i = 2; i + 1 < last; i += 2) {
    if (match_label(input, v[i], attrs, zoom)) {
      return eval_json(v[i + 1], attrs, zoom, out);
    }
  }
  return eval_json(v[last], attrs, zoom, out);
}

bool eval_case(const rapidjson::Value& v, const AttrMap& attrs, double zoom,
               ExprValue* out) {
  // ["case", cond0, out0, cond1, out1, ..., default]
  if (v.Size() < 4 || (v.Size() % 2) != 0) {
    return false;
  }
  const rapidjson::SizeType last = v.Size() - 1;
  for (rapidjson::SizeType i = 1; i + 1 < last; i += 2) {
    ExprValue cond;
    if (!eval_json(v[i], attrs, zoom, &cond)) {
      return false;
    }
    if (cond.as_bool()) {
      return eval_json(v[i + 1], attrs, zoom, out);
    }
  }
  return eval_json(v[last], attrs, zoom, out);
}

bool eval_coalesce(const rapidjson::Value& v, const AttrMap& attrs, double zoom,
                   ExprValue* out) {
  if (v.Size() < 2) {
    return false;
  }
  for (rapidjson::SizeType i = 1; i < v.Size(); ++i) {
    ExprValue cur;
    if (!eval_json(v[i], attrs, zoom, &cur)) {
      return false;
    }
    if (cur.kind != ExprValue::Kind::kNull) {
      *out = cur;
      return true;
    }
  }
  out->kind = ExprValue::Kind::kNull;
  return true;
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
  if (op == "interpolate") {
    return eval_interpolate(v, attrs, zoom, out);
  }
  if (op == "step") {
    return eval_step(v, attrs, zoom, out);
  }
  if (op == "match") {
    return eval_match(v, attrs, zoom, out);
  }
  if (op == "case") {
    return eval_case(v, attrs, zoom, out);
  }
  if (op == "coalesce") {
    return eval_coalesce(v, attrs, zoom, out);
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
