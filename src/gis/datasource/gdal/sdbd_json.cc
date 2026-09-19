// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/sdbd_json.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace gis {
namespace datasource {
namespace {

class Parser {
 public:
  explicit Parser(const std::string& text) : text_(text) {}

  bool parse(Json* out) {
    skip();
    if (!parse_value(out)) {
      return false;
    }
    skip();
    return pos_ == text_.size();
  }

  const std::string& error() const { return error_; }

 private:
  const std::string& text_;
  std::size_t pos_ = 0;
  std::string error_;

  void fail(const char* msg) {
    if (error_.empty()) {
      error_ = msg;
    }
  }

  void skip() {
    while (pos_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
  }

  bool eat(char c) {
    skip();
    if (pos_ < text_.size() && text_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  bool parse_value(Json* out) {
    skip();
    if (pos_ >= text_.size()) {
      fail("unexpected end");
      return false;
    }
    const char c = text_[pos_];
    if (c == '{') {
      return parse_object(out);
    }
    if (c == '[') {
      return parse_array(out);
    }
    if (c == '"') {
      return parse_string(&out->s) && ((out->type = Json::kString), true);
    }
    if (c == 't' || c == 'f') {
      return parse_bool(out);
    }
    if (c == 'n') {
      return parse_null(out);
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return parse_number(out);
    }
    fail("invalid value");
    return false;
  }

  bool parse_object(Json* out) {
    if (!eat('{')) {
      fail("expected '{'");
      return false;
    }
    out->type = Json::kObject;
    skip();
    if (eat('}')) {
      return true;
    }
    for (;;) {
      std::string key;
      if (!parse_string(&key)) {
        return false;
      }
      if (!eat(':')) {
        fail("expected ':'");
        return false;
      }
      Json value;
      if (!parse_value(&value)) {
        return false;
      }
      out->o.emplace(std::move(key), std::move(value));
      skip();
      if (eat('}')) {
        return true;
      }
      if (!eat(',')) {
        fail("expected ',' or '}'");
        return false;
      }
    }
  }

  bool parse_array(Json* out) {
    if (!eat('[')) {
      fail("expected '['");
      return false;
    }
    out->type = Json::kArray;
    skip();
    if (eat(']')) {
      return true;
    }
    for (;;) {
      Json value;
      if (!parse_value(&value)) {
        return false;
      }
      out->a.push_back(std::move(value));
      skip();
      if (eat(']')) {
        return true;
      }
      if (!eat(',')) {
        fail("expected ',' or ']'");
        return false;
      }
    }
  }

  bool parse_string(std::string* out) {
    skip();
    if (!eat('"')) {
      fail("expected string");
      return false;
    }
    out->clear();
    while (pos_ < text_.size()) {
      char c = text_[pos_++];
      if (c == '"') {
        return true;
      }
      if (c == '\\') {
        if (pos_ >= text_.size()) {
          fail("bad escape");
          return false;
        }
        const char e = text_[pos_++];
        switch (e) {
          case '"':
          case '\\':
          case '/':
            out->push_back(e);
            break;
          case 'b':
            out->push_back('\b');
            break;
          case 'f':
            out->push_back('\f');
            break;
          case 'n':
            out->push_back('\n');
            break;
          case 'r':
            out->push_back('\r');
            break;
          case 't':
            out->push_back('\t');
            break;
          case 'u':
            if (pos_ + 4 > text_.size()) {
              fail("bad unicode escape");
              return false;
            }
            pos_ += 4;
            out->push_back('?');
            break;
          default:
            out->push_back(e);
            break;
        }
        continue;
      }
      out->push_back(c);
    }
    fail("unterminated string");
    return false;
  }

  bool parse_number(Json* out) {
    skip();
    const std::size_t start = pos_;
    if (pos_ < text_.size() && text_[pos_] == '-') {
      ++pos_;
    }
    while (pos_ < text_.size() &&
           std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
    if (pos_ < text_.size() && text_[pos_] == '.') {
      ++pos_;
      while (pos_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
        ++pos_;
      }
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
        ++pos_;
      }
      while (pos_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
        ++pos_;
      }
    }
    out->type = Json::kNumber;
    out->s = text_.substr(start, pos_ - start);
    out->n = std::strtod(out->s.c_str(), nullptr);
    return true;
  }

  bool parse_bool(Json* out) {
    skip();
    if (text_.compare(pos_, 4, "true") == 0) {
      pos_ += 4;
      out->type = Json::kBool;
      out->b = true;
      return true;
    }
    if (text_.compare(pos_, 5, "false") == 0) {
      pos_ += 5;
      out->type = Json::kBool;
      out->b = false;
      return true;
    }
    fail("expected bool");
    return false;
  }

  bool parse_null(Json* out) {
    skip();
    if (text_.compare(pos_, 4, "null") == 0) {
      pos_ += 4;
      out->type = Json::kNull;
      return true;
    }
    fail("expected null");
    return false;
  }
};

bool looks_like_json_key(const std::string& body, const char* key) {
  const std::string needle = std::string("\"") + key + "\"";
  std::size_t pos = 0;
  while ((pos = body.find(needle, pos)) != std::string::npos) {
    std::size_t colon = pos + needle.size();
    while (colon < body.size() &&
           std::isspace(static_cast<unsigned char>(body[colon]))) {
      ++colon;
    }
    if (colon < body.size() && body[colon] == ':') {
      return true;
    }
    pos += needle.size();
  }
  return false;
}

std::string id_from_json(const Json& value) {
  if (value.type == Json::kString) {
    return value.s;
  }
  if (value.type == Json::kNumber) {
    if (std::fabs(value.n - std::llround(value.n)) < 1e-9) {
      return std::to_string(static_cast<long long>(std::llround(value.n)));
    }
    return value.s.empty() ? std::to_string(value.n) : value.s;
  }
  return {};
}

BBox parse_bbox(const Json& json) {
  BBox box;
  box.min_x = json.number("min_x");
  box.min_y = json.number("min_y");
  box.max_x = json.number("max_x");
  box.max_y = json.number("max_y");
  return box;
}

void write_attrs(std::ostringstream& o,
                 const std::map<std::string, std::string>& attrs) {
  o << '{';
  bool first = true;
  for (const auto& kv : attrs) {
    if (!first) {
      o << ',';
    }
    first = false;
    o << '"' << json_escape(kv.first) << "\":\"" << json_escape(kv.second)
      << '"';
  }
  o << '}';
}

}  // namespace

bool Json::has(const char* key) const { return o.find(key) != o.end(); }

const Json* Json::get(const char* key) const {
  auto it = o.find(key);
  if (it == o.end()) {
    return nullptr;
  }
  return &it->second;
}

std::string Json::string(const char* key, const std::string& def) const {
  const Json* v = get(key);
  if (!v) {
    return def;
  }
  if (v->type == Json::kString) {
    return v->s;
  }
  if (v->type == Json::kNumber) {
    return id_from_json(*v);
  }
  return def;
}

double Json::number(const char* key, double def) const {
  const Json* v = get(key);
  if (!v || v->type != Json::kNumber) {
    return def;
  }
  return v->n;
}

bool Json::boolean(const char* key, bool def) const {
  const Json* v = get(key);
  if (!v) {
    return def;
  }
  if (v->type == Json::kBool) {
    return v->b;
  }
  return def;
}

std::string Json::as_string() const {
  if (type == Json::kString) {
    return s;
  }
  if (type == Json::kNumber) {
    return id_from_json(*this);
  }
  if (type == Json::kBool) {
    return b ? "true" : "false";
  }
  return {};
}

bool parse_json(const std::string& text, Json* out, std::string* err) {
  if (!out) {
    return false;
  }
  *out = Json{};
  Parser parser(text);
  if (!parser.parse(out)) {
    if (err) {
      *err = parser.error().empty() ? "bad_request" : parser.error();
    }
    return false;
  }
  return true;
}

std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(static_cast<char>(c));
        break;
    }
  }
  return out;
}

std::string open_request_to_json(const OpenRequest& req) {
  std::ostringstream o;
  o << "{\"layer\":\"" << json_escape(req.layer) << "\",\"columns\":[";
  for (std::size_t i = 0; i < req.columns.size(); ++i) {
    if (i) {
      o << ',';
    }
    o << '"' << json_escape(req.columns[i]) << '"';
  }
  o << "],\"bbox\":{\"min_x\":" << req.bbox.min_x
    << ",\"min_y\":" << req.bbox.min_y << ",\"max_x\":" << req.bbox.max_x
    << ",\"max_y\":" << req.bbox.max_y << "},\"predicate\":\""
    << json_escape(req.predicate) << '"';
  if (!req.crs.empty()) {
    o << ",\"crs\":\"" << json_escape(req.crs) << '"';
  }
  o << ",\"limit\":" << req.limit << '}';
  return o.str();
}

std::string layer_info_to_json(const LayerInfo& info) {
  std::ostringstream o;
  o << "{\"name\":\"" << json_escape(info.name) << "\",\"crs\":\""
    << json_escape(info.crs) << "\",\"geom_type\":\""
    << json_escape(info.geom_type) << "\",\"geom_column\":\""
    << json_escape(info.geom_column)
    << "\",\"extent\":{\"min_x\":" << info.extent.min_x
    << ",\"min_y\":" << info.extent.min_y << ",\"max_x\":" << info.extent.max_x
    << ",\"max_y\":" << info.extent.max_y << "},\"fields\":[";
  for (std::size_t i = 0; i < info.fields.size(); ++i) {
    if (i) {
      o << ',';
    }
    o << "{\"name\":\"" << json_escape(info.fields[i].name) << "\",\"type\":\""
      << json_escape(info.fields[i].type) << "\"}";
  }
  o << "]}";
  return o.str();
}

std::string feature_set_to_json(const FeatureSet& set) {
  std::ostringstream o;
  o << "{\"layer\":\"" << json_escape(set.layer) << "\",\"crs\":\""
    << json_escape(set.crs) << "\",\"count\":" << set.count
    << ",\"offset\":" << set.offset << ",\"features\":[";
  for (std::size_t i = 0; i < set.features.size(); ++i) {
    if (i) {
      o << ',';
    }
    const Feature& f = set.features[i];
    o << "{\"id\":\"" << json_escape(f.id) << "\",\"geom_wkt\":\""
      << json_escape(f.geom_wkt) << "\",\"attrs\":";
    write_attrs(o, f.attrs);
    o << '}';
  }
  o << "]}";
  return o.str();
}

bool parse_layer_info(const Json& json, LayerInfo* out) {
  if (!out || json.type != Json::kObject) {
    return false;
  }
  out->name = json.string("name");
  out->crs = json.string("crs");
  out->geom_type = json.string("geom_type");
  out->geom_column = json.string("geom_column");
  if (const Json* extent = json.get("extent")) {
    out->extent = parse_bbox(*extent);
  }
  out->fields.clear();
  if (const Json* fields = json.get("fields")) {
    if (fields->type == Json::kArray) {
      for (const auto& f : fields->a) {
        FieldInfo info;
        info.name = f.string("name");
        info.type = f.string("type");
        if (!info.name.empty()) {
          out->fields.push_back(std::move(info));
        }
      }
    }
  }
  return !out->name.empty();
}

bool parse_layer_info(const std::string& text, LayerInfo* out,
                      std::string* err) {
  Json json;
  if (!parse_json(text, &json, err)) {
    return false;
  }
  return parse_layer_info(json, out);
}

bool parse_catalog(const std::string& text, std::vector<LayerInfo>* out,
                   std::string* err) {
  if (!out) {
    return false;
  }
  Json json;
  if (!parse_json(text, &json, err)) {
    return false;
  }
  if (json.type != Json::kArray) {
    if (err) {
      *err = "catalog must be a JSON array";
    }
    return false;
  }
  out->clear();
  for (const auto& item : json.a) {
    LayerInfo info;
    if (!parse_layer_info(item, &info)) {
      if (err) {
        *err = "invalid LayerInfo";
      }
      return false;
    }
    out->push_back(std::move(info));
  }
  return true;
}

bool parse_feature_set(const std::string& text, FeatureSet* out,
                       std::string* err) {
  if (!out) {
    return false;
  }
  Json json;
  if (!parse_json(text, &json, err)) {
    return false;
  }
  if (json.type != Json::kObject) {
    if (err) {
      *err = "FeatureSet must be an object";
    }
    return false;
  }
  out->layer = json.string("layer");
  out->crs = json.string("crs");
  out->count = static_cast<int>(json.number("count"));
  out->offset = static_cast<int>(json.number("offset"));
  out->features.clear();
  const Json* features = json.get("features");
  if (features && features->type == Json::kArray) {
    for (const auto& item : features->a) {
      Feature feature;
      if (const Json* id = item.get("id")) {
        feature.id = id_from_json(*id);
      }
      feature.geom_wkt = item.string("geom_wkt");
      if (const Json* attrs = item.get("attrs")) {
        if (attrs->type == Json::kObject) {
          for (const auto& kv : attrs->o) {
            feature.attrs[kv.first] = kv.second.as_string();
          }
        }
      }
      out->features.push_back(std::move(feature));
    }
  }
  if (out->count == 0) {
    out->count = static_cast<int>(out->features.size());
  }
  out->move_first();
  return true;
}

bool parse_error_body(const std::string& text, SdbdResult* out) {
  if (!out) {
    return false;
  }
  Json json;
  if (!parse_json(text, &json, nullptr) || json.type != Json::kObject) {
    return false;
  }
  out->ok = json.boolean("ok", false);
  out->error = json.string("error");
  out->message = json.string("message");
  return !out->error.empty() || json.has("ok");
}

bool body_has_forbidden_sql(const std::string& body) {
  Json json;
  if (parse_json(body, &json, nullptr) && json.type == Json::kObject) {
    return json.has("sql") || json.has("raw_sql") || json.has("raw_query");
  }
  return looks_like_json_key(body, "sql") ||
         looks_like_json_key(body, "raw_sql") ||
         looks_like_json_key(body, "raw_query");
}

}  // namespace datasource
}  // namespace gis
