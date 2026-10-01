// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/manifest/manifest.h"

#include <cctype>
#include <set>
#include <utility>

#include <rapidjson/document.h>

namespace plugin {
namespace {

using JsonValue = rapidjson::Value;

bool is_plugin_id(std::string_view id) {
  if (id.empty() || id.size() > 64) {
    return false;
  }
  size_t i = 0;
  auto consume_first = [&]() {
    if (i >= id.size() ||
        (!std::islower(static_cast<unsigned char>(id[i])) &&
         !std::isdigit(static_cast<unsigned char>(id[i])))) {
      return false;
    }
    while (i < id.size() &&
           (std::islower(static_cast<unsigned char>(id[i])) ||
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

const JsonValue* member(const JsonValue& obj, const char* key) {
  if (!obj.IsObject()) {
    return nullptr;
  }
  const auto it = obj.FindMember(key);
  return it == obj.MemberEnd() ? nullptr : &it->value;
}

bool take_string(const JsonValue& obj,
                 const char* key,
                 std::string* out,
                 bool req,
                 size_t max_len,
                 std::string* err) {
  const JsonValue* v = member(obj, key);
  if (!v) {
    if (req) {
      if (err) {
        *err = std::string("missing ") + key;
      }
      return false;
    }
    return true;
  }
  if (!v->IsString()) {
    if (err) {
      *err = std::string("bad type ") + key;
    }
    return false;
  }
  if (v->GetStringLength() > max_len) {
    if (err) {
      *err = std::string("too long ") + key;
    }
    return false;
  }
  *out = v->GetString();
  return true;
}

bool parse_contrib_array(const JsonValue* arr,
                         auto&& add,
                         std::set<std::string>* ids,
                         std::string* err) {
  if (!arr) {
    return true;
  }
  if (!arr->IsArray()) {
    if (err) {
      *err = "contributes array expected";
    }
    return false;
  }
  for (const JsonValue& item : arr->GetArray()) {
    if (!item.IsObject()) {
      if (err) {
        *err = "contribution must be object";
      }
      return false;
    }
    const JsonValue* idj = member(item, "id");
    if (!idj || !idj->IsString() || !is_contrib_id(idj->GetString())) {
      if (err) {
        *err = "bad contribution id";
      }
      return false;
    }
    if (!ids->insert(idj->GetString()).second) {
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

bool fill_manifest(const JsonValue& root, Manifest* out, std::string* err) {
  Manifest m;
  if (!take_string(root, "id", &m.id, true, 64, err)) {
    return false;
  }
  if (!is_plugin_id(m.id)) {
    *err = "invalid id";
    return false;
  }
  if (!take_string(root, "name", &m.name, true, 80, err)) {
    return false;
  }
  if (m.name.empty()) {
    *err = "empty name";
    return false;
  }
  if (!take_string(root, "version", &m.version, true, 32, err)) {
    return false;
  }
  if (!is_semver(m.version)) {
    *err = "bad version";
    return false;
  }

  const JsonValue* api = member(root, "api_version");
  if (!api || !api->IsNumber()) {
    *err = "missing api_version";
    return false;
  }
  m.api_version = api->GetInt();

  std::string kind_s;
  if (!take_string(root, "kind", &kind_s, true, 32, err)) {
    return false;
  }
  if (!kind_from_string(kind_s, &m.kind)) {
    *err = "bad kind";
    return false;
  }

  if (!take_string(root, "author", &m.author, false, 80, err) ||
      !take_string(root, "description", &m.description, false, 400, err) ||
      !take_string(root, "homepage", &m.homepage, false, 256, err) ||
      !take_string(root, "min_host_version", &m.min_host_version, false, 32,
                   err) ||
      !take_string(root, "library", &m.library, false, 128, err) ||
      !take_string(root, "entry", &m.entry, false, 128, err)) {
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

  const JsonValue* contrib = member(root, "contributes");
  if (contrib) {
    if (!contrib->IsObject()) {
      *err = "contributes must be object";
      return false;
    }
    std::set<std::string> ids;
    auto title_of = [&](const JsonValue& item, std::string* title) {
      const JsonValue* t = member(item, "title");
      if (t && t->IsString()) {
        *title = t->GetString();
      }
      return true;
    };
    if (!parse_contrib_array(
            member(*contrib, "commands"),
            [&](const JsonValue& item) {
              CommandContrib c;
              c.id = member(item, "id")->GetString();
              title_of(item, &c.title);
              const JsonValue* menu = member(item, "menu");
              if (menu && menu->IsString()) {
                c.menu = menu->GetString();
              }
              m.contributes.commands.push_back(std::move(c));
              return true;
            },
            &ids, err)) {
      return false;
    }
    if (!parse_contrib_array(
            member(*contrib, "menus"),
            [&](const JsonValue& item) {
              MenuContrib c;
              c.id = member(item, "id")->GetString();
              title_of(item, &c.title);
              const JsonValue* parent = member(item, "parent");
              if (parent && parent->IsString()) {
                c.parent = parent->GetString();
              }
              m.contributes.menus.push_back(std::move(c));
              return true;
            },
            &ids, err)) {
      return false;
    }
    if (!parse_contrib_array(
            member(*contrib, "docks"),
            [&](const JsonValue& item) {
              DockContrib c;
              c.id = member(item, "id")->GetString();
              title_of(item, &c.title);
              const JsonValue* area = member(item, "area");
              if (area && area->IsString()) {
                c.area = area->GetString();
              }
              m.contributes.docks.push_back(std::move(c));
              return true;
            },
            &ids, err)) {
      return false;
    }
    if (!parse_contrib_array(
            member(*contrib, "dialogs"),
            [&](const JsonValue& item) {
              DialogContrib c;
              c.id = member(item, "id")->GetString();
              title_of(item, &c.title);
              m.contributes.dialogs.push_back(std::move(c));
              return true;
            },
            &ids, err)) {
      return false;
    }
    if (!parse_contrib_array(
            member(*contrib, "processing"),
            [&](const JsonValue& item) {
              ProcessingContrib c;
              c.id = member(item, "id")->GetString();
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

}  // namespace

bool parse_manifest(std::string_view json, Manifest* out, std::string* err) {
  if (!out) {
    if (err) {
      *err = "null out";
    }
    return false;
  }
  std::string local_err;
  if (!err) {
    err = &local_err;
  }

  rapidjson::Document root;
  root.Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  if (root.HasParseError() || !root.IsObject()) {
    *err = root.HasParseError() ? "JSON parse error" : "root must be object";
    return false;
  }
  return fill_manifest(root, out, err);
}

}  // namespace plugin
