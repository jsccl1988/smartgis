// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/manifest.h"

#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>

namespace plugin {
namespace detail {
namespace {

struct Json {
  enum class Kind { kNull, kBool, kNumber, kString, kArray, kObject };
  Kind kind = Kind::kNull;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<Json> a;
  std::map<std::string, Json> o;

  const Json* get(const char* key) const {
    auto it = o.find(key);
    return it == o.end() ? nullptr : &it->second;
  }
};

class Parser {
 public:
  explicit Parser(std::string_view in) : in_(in) {}

  bool parse(Json* out, std::string* err) {
    skip();
    if (!parse_value(out, err)) {
      return false;
    }
    skip();
    if (pos_ != in_.size()) {
      return fail(err, "trailing JSON");
    }
    return true;
  }

 private:
  std::string_view in_;
  size_t pos_ = 0;

  static bool fail(std::string* err, const char* msg) {
    if (err) {
      *err = msg;
    }
    return false;
  }

  void skip() {
    while (pos_ < in_.size() &&
           std::isspace(static_cast<unsigned char>(in_[pos_]))) {
      ++pos_;
    }
  }

  bool peek(char c) {
    skip();
    return pos_ < in_.size() && in_[pos_] == c;
  }

  bool eat(char c, std::string* err) {
    skip();
    if (pos_ >= in_.size() || in_[pos_] != c) {
      return fail(err, "unexpected token");
    }
    ++pos_;
    return true;
  }

  bool parse_value(Json* out, std::string* err) {
    skip();
    if (pos_ >= in_.size()) {
      return fail(err, "unexpected end");
    }
    const char c = in_[pos_];
    if (c == '{') {
      return parse_object(out, err);
    }
    if (c == '[') {
      return parse_array(out, err);
    }
    if (c == '"') {
      out->kind = Json::Kind::kString;
      return parse_string(&out->s, err);
    }
    if (c == 't' || c == 'f') {
      return parse_bool(out, err);
    }
    if (c == 'n') {
      return parse_null(out, err);
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return parse_number(out, err);
    }
    return fail(err, "unexpected token");
  }

  bool parse_null(Json* out, std::string* err) {
    if (in_.substr(pos_, 4) != "null") {
      return fail(err, "bad null");
    }
    pos_ += 4;
    out->kind = Json::Kind::kNull;
    return true;
  }

  bool parse_bool(Json* out, std::string* err) {
    if (in_.substr(pos_, 4) == "true") {
      pos_ += 4;
      out->kind = Json::Kind::kBool;
      out->b = true;
      return true;
    }
    if (in_.substr(pos_, 5) == "false") {
      pos_ += 5;
      out->kind = Json::Kind::kBool;
      out->b = false;
      return true;
    }
    return fail(err, "bad bool");
  }

  bool parse_number(Json* out, std::string* err) {
    const size_t start = pos_;
    if (pos_ < in_.size() && in_[pos_] == '-') {
      ++pos_;
    }
    if (pos_ >= in_.size() ||
        !std::isdigit(static_cast<unsigned char>(in_[pos_]))) {
      return fail(err, "bad number");
    }
    while (pos_ < in_.size() &&
           std::isdigit(static_cast<unsigned char>(in_[pos_]))) {
      ++pos_;
    }
    if (pos_ < in_.size() && in_[pos_] == '.') {
      ++pos_;
      while (pos_ < in_.size() &&
             std::isdigit(static_cast<unsigned char>(in_[pos_]))) {
        ++pos_;
      }
    }
    char* end = nullptr;
    const std::string tok(in_.substr(start, pos_ - start));
    out->n = std::strtod(tok.c_str(), &end);
    out->kind = Json::Kind::kNumber;
    return true;
  }

  bool parse_string(std::string* out, std::string* err) {
    if (!eat('"', err)) {
      return false;
    }
    out->clear();
    while (pos_ < in_.size()) {
      const char c = in_[pos_++];
      if (c == '"') {
        return true;
      }
      if (c == '\\') {
        if (pos_ >= in_.size()) {
          return fail(err, "bad escape");
        }
        const char e = in_[pos_++];
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
            if (pos_ + 4 > in_.size()) {
              return fail(err, "bad unicode escape");
            }
            pos_ += 4;
            out->push_back('?');
            break;
          default:
            return fail(err, "bad escape");
        }
        continue;
      }
      if (static_cast<unsigned char>(c) < 0x20) {
        return fail(err, "control in string");
      }
      out->push_back(c);
    }
    return fail(err, "unterminated string");
  }

  bool parse_array(Json* out, std::string* err) {
    if (!eat('[', err)) {
      return false;
    }
    out->kind = Json::Kind::kArray;
    skip();
    if (peek(']')) {
      ++pos_;
      return true;
    }
    while (true) {
      Json item;
      if (!parse_value(&item, err)) {
        return false;
      }
      out->a.push_back(std::move(item));
      skip();
      if (peek(']')) {
        ++pos_;
        return true;
      }
      if (!eat(',', err)) {
        return false;
      }
    }
  }

  bool parse_object(Json* out, std::string* err) {
    if (!eat('{', err)) {
      return false;
    }
    out->kind = Json::Kind::kObject;
    skip();
    if (peek('}')) {
      ++pos_;
      return true;
    }
    while (true) {
      skip();
      std::string key;
      if (!parse_string(&key, err)) {
        return false;
      }
      if (!eat(':', err)) {
        return false;
      }
      Json val;
      if (!parse_value(&val, err)) {
        return false;
      }
      out->o.emplace(std::move(key), std::move(val));
      skip();
      if (peek('}')) {
        ++pos_;
        return true;
      }
      if (!eat(',', err)) {
        return false;
      }
    }
  }
};

bool is_plugin_id(std::string_view id) {
  if (id.empty() || id.size() > 64) {
    return false;
  }
  size_t i = 0;
  auto consume_first = [&]() {
    if (i >= id.size() || !std::islower(static_cast<unsigned char>(id[i])) &&
                              !std::isdigit(static_cast<unsigned char>(id[i]))) {
      return false;
    }
    while (i < id.size() && (std::islower(static_cast<unsigned char>(id[i])) ||
                             std::isdigit(static_cast<unsigned char>(id[i])))) {
      ++i;
    }
    return true;
  };
  auto consume_rest = [&]() {
    if (i >= id.size() || id[i] != '.') {
      return false;
    }
    ++i;
    if (i >= id.size()) {
      return false;
    }
    const unsigned char c = static_cast<unsigned char>(id[i]);
    if (!std::islower(c) && !std::isdigit(c) && id[i] != '_' && id[i] != '-') {
      return false;
    }
    while (i < id.size()) {
      const unsigned char d = static_cast<unsigned char>(id[i]);
      if (!std::islower(d) && !std::isdigit(d) && id[i] != '_' &&
          id[i] != '-') {
        break;
      }
      ++i;
    }
    return true;
  };
  if (!consume_first()) {
    return false;
  }
  if (!consume_rest()) {
    return false;
  }
  while (i < id.size()) {
    if (!consume_rest()) {
      return false;
    }
  }
  return i == id.size();
}

bool is_semver(std::string_view v) {
  int parts = 0;
  size_t i = 0;
  while (i < v.size()) {
    if (parts > 0) {
      if (v[i] != '.') {
        return false;
      }
      ++i;
    }
    if (i >= v.size() || !std::isdigit(static_cast<unsigned char>(v[i]))) {
      return false;
    }
    while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) {
      ++i;
    }
    ++parts;
  }
  return parts == 3;
}

bool is_contrib_id(std::string_view id) {
  if (id.empty()) {
    return false;
  }
  for (char c : id) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (!std::islower(u) && !std::isdigit(u) && c != '.' && c != '_') {
      return false;
    }
  }
  return true;
}

bool kind_from_string(std::string_view s, PluginKind* out) {
  if (s == "builtin") {
    *out = PluginKind::kBuiltin;
    return true;
  }
  if (s == "native") {
    *out = PluginKind::kNative;
    return true;
  }
  if (s == "python") {
    *out = PluginKind::kPython;
    return true;
  }
  if (s == "legacy_am") {
    *out = PluginKind::kLegacyAm;
    return true;
  }
  return false;
}

bool take_string(const Json& obj, const char* key, std::string* out, bool req,
                 size_t max_len, std::string* err) {
  const Json* v = obj.get(key);
  if (!v) {
    if (req) {
      if (err) {
        *err = std::string("missing ") + key;
      }
      return false;
    }
    return true;
  }
  if (v->kind != Json::Kind::kString) {
    if (err) {
      *err = std::string("bad type ") + key;
    }
    return false;
  }
  if (v->s.size() > max_len) {
    if (err) {
      *err = std::string("too long ") + key;
    }
    return false;
  }
  *out = v->s;
  return true;
}

bool parse_contrib_array(const Json* arr, auto&& add, std::set<std::string>* ids,
                         std::string* err) {
  if (!arr) {
    return true;
  }
  if (arr->kind != Json::Kind::kArray) {
    if (err) {
      *err = "contributes array expected";
    }
    return false;
  }
  for (const Json& item : arr->a) {
    if (item.kind != Json::Kind::kObject) {
      if (err) {
        *err = "contribution must be object";
      }
      return false;
    }
    const Json* idj = item.get("id");
    if (!idj || idj->kind != Json::Kind::kString ||
        !is_contrib_id(idj->s)) {
      if (err) {
        *err = "bad contribution id";
      }
      return false;
    }
    if (!ids->insert(idj->s).second) {
      if (err) {
        *err = "duplicate contribution id";
      }
      return false;
    }
    if (!add(item)) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool parse_json(std::string_view in, Json* out, std::string* err) {
  Parser p(in);
  return p.parse(out, err);
}

}  // namespace detail

bool parse_manifest(std::string_view json, Manifest* out, std::string* err) {
  if (!out) {
    if (err) {
      *err = "null out";
    }
    return false;
  }
  detail::Json root;
  std::string local_err;
  if (!err) {
    err = &local_err;
  }
  if (!detail::parse_json(json, &root, err)) {
    return false;
  }
  if (root.kind != detail::Json::Kind::kObject) {
    *err = "root must be object";
    return false;
  }

  Manifest m;
  if (!detail::take_string(root, "id", &m.id, true, 64, err)) {
    return false;
  }
  if (!detail::is_plugin_id(m.id)) {
    *err = "invalid id";
    return false;
  }
  if (!detail::take_string(root, "name", &m.name, true, 80, err)) {
    return false;
  }
  if (m.name.empty()) {
    *err = "empty name";
    return false;
  }
  if (!detail::take_string(root, "version", &m.version, true, 32, err)) {
    return false;
  }
  if (!detail::is_semver(m.version)) {
    *err = "bad version";
    return false;
  }

  const detail::Json* api = root.get("api_version");
  if (!api || api->kind != detail::Json::Kind::kNumber) {
    *err = "missing api_version";
    return false;
  }
  m.api_version = static_cast<int>(api->n);

  std::string kind_s;
  if (!detail::take_string(root, "kind", &kind_s, true, 32, err)) {
    return false;
  }
  if (!detail::kind_from_string(kind_s, &m.kind)) {
    *err = "bad kind";
    return false;
  }

  if (!detail::take_string(root, "author", &m.author, false, 80, err) ||
      !detail::take_string(root, "description", &m.description, false, 400,
                           err) ||
      !detail::take_string(root, "homepage", &m.homepage, false, 256, err) ||
      !detail::take_string(root, "min_host_version", &m.min_host_version, false,
                           32, err) ||
      !detail::take_string(root, "library", &m.library, false, 128, err) ||
      !detail::take_string(root, "entry", &m.entry, false, 128, err)) {
    return false;
  }

  if (m.kind != PluginKind::kLegacyAm && m.api_version != 2) {
    *err = "api_version must be 2";
    return false;
  }
  if (m.kind == PluginKind::kPython && m.entry.empty()) {
    *err = "python entry required";
    return false;
  }

  const detail::Json* contrib = root.get("contributes");
  if (contrib) {
    if (contrib->kind != detail::Json::Kind::kObject) {
      *err = "contributes must be object";
      return false;
    }
    std::set<std::string> ids;
    auto title_of = [&](const detail::Json& item, std::string* title) {
      const detail::Json* t = item.get("title");
      if (t && t->kind == detail::Json::Kind::kString) {
        *title = t->s;
      }
      return true;
    };
    if (!detail::parse_contrib_array(
            contrib->get("commands"),
            [&](const detail::Json& item) {
              CommandContrib c;
              c.id = item.get("id")->s;
              title_of(item, &c.title);
              const detail::Json* menu = item.get("menu");
              if (menu && menu->kind == detail::Json::Kind::kString) {
                c.menu = menu->s;
              }
              m.contributes.commands.push_back(std::move(c));
              return true;
            },
            &ids, err)) {
      return false;
    }
    if (!detail::parse_contrib_array(
            contrib->get("menus"),
            [&](const detail::Json& item) {
              MenuContrib c;
              c.id = item.get("id")->s;
              title_of(item, &c.title);
              const detail::Json* parent = item.get("parent");
              if (parent && parent->kind == detail::Json::Kind::kString) {
                c.parent = parent->s;
              }
              m.contributes.menus.push_back(std::move(c));
              return true;
            },
            &ids, err)) {
      return false;
    }
    if (!detail::parse_contrib_array(
            contrib->get("docks"),
            [&](const detail::Json& item) {
              DockContrib c;
              c.id = item.get("id")->s;
              title_of(item, &c.title);
              const detail::Json* area = item.get("area");
              if (area && area->kind == detail::Json::Kind::kString) {
                c.area = area->s;
              }
              m.contributes.docks.push_back(std::move(c));
              return true;
            },
            &ids, err)) {
      return false;
    }
    if (!detail::parse_contrib_array(
            contrib->get("dialogs"),
            [&](const detail::Json& item) {
              DialogContrib c;
              c.id = item.get("id")->s;
              title_of(item, &c.title);
              m.contributes.dialogs.push_back(std::move(c));
              return true;
            },
            &ids, err)) {
      return false;
    }
    if (!detail::parse_contrib_array(
            contrib->get("processing"),
            [&](const detail::Json& item) {
              ProcessingContrib c;
              c.id = item.get("id")->s;
              title_of(item, &c.title);
              m.contributes.processing.push_back(std::move(c));
              return true;
            },
            &ids, err)) {
      return false;
    }
  }

  *out = std::move(m);
  return true;
}

}  // namespace plugin
