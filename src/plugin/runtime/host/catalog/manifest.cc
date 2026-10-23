// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/catalog/manifest.h"

#include <cctype>
#include <set>
#include <string_view>
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

bool normalize_startup_viewport(std::string_view in, std::string* out,
                                std::string* err) {
  if (in.empty()) {
    out->clear();
    return true;
  }
  if (in == "map2d" || in == "map" || in == "2d") {
    *out = "map2d";
    return true;
  }
  if (in == "scene3d" || in == "scene" || in == "3d") {
    *out = "scene3d";
    return true;
  }
  if (err) {
    *err = "bad startup.viewport";
  }
  return false;
}

bool parse_startup(const JsonValue& root, ManifestStartup* out, std::string* err) {
  const JsonValue* st = member(root, "startup");
  if (!st) {
    return true;
  }
  if (!st->IsObject()) {
    if (err) {
      *err = "startup must be object";
    }
    return false;
  }
  const JsonValue* act = member(*st, "activate");
  if (act) {
    if (!act->IsBool()) {
      if (err) {
        *err = "startup.activate must be bool";
      }
      return false;
    }
    out->activate = act->GetBool();
  }
  std::string vp;
  if (!take_string(*st, "viewport", &vp, false, 32, err)) {
    return false;
  }
  if (!normalize_startup_viewport(vp, &out->viewport, err)) {
    return false;
  }
  if (!take_string(*st, "seed", &out->seed, false, 512, err)) {
    return false;
  }
  if (!take_string(*st, "scenario", &out->scenario, false, 80, err)) {
    return false;
  }
  if (!out->scenario.empty() && !is_contrib_id(out->scenario)) {
    if (err) {
      *err = "bad startup.scenario";
    }
    return false;
  }
  std::string present;
  if (!take_string(*st, "present", &present, false, 32, err)) {
    return false;
  }
  if (!present.empty()) {
    if (present == "preview" || present == "map_preview" ||
        present == "world_preview") {
      out->present = "preview";
    } else if (present == "main") {
      out->present = "main";
    } else {
      if (err) {
        *err = "bad startup.present";
      }
      return false;
    }
  }
  if (!take_string(*st, "fields", &out->fields, false, 512, err)) {
    return false;
  }
  const JsonValue* pri = member(*st, "priority");
  if (pri) {
    if (!pri->IsInt() && !pri->IsUint() && !pri->IsNumber()) {
      if (err) {
        *err = "startup.priority must be int";
      }
      return false;
    }
    out->priority = pri->GetInt();
  }
  const JsonValue* cmds = member(*st, "commands");
  if (!cmds) {
    return true;
  }
  if (!cmds->IsArray()) {
    if (err) {
      *err = "startup.commands must be array";
    }
    return false;
  }
  for (const JsonValue& item : cmds->GetArray()) {
    if (!item.IsString() || item.GetStringLength() > 128 ||
        !is_contrib_id(item.GetString())) {
      if (err) {
        *err = "bad startup.commands id";
      }
      return false;
    }
    out->commands.emplace_back(item.GetString());
  }
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
    // Ids must be unique within a contrib kind. The same id is reused across
    // commands / dialogs / processing (flood.inundate is all three).
    std::set<std::string> command_ids;
    std::set<std::string> menu_ids;
    std::set<std::string> dock_ids;
    std::set<std::string> dialog_ids;
    std::set<std::string> processing_ids;
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
            &command_ids, err)) {
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
            &menu_ids, err)) {
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
            &dock_ids, err)) {
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
            &dialog_ids, err)) {
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
            &processing_ids, err)) {
      return false;
    }
  }

  if (!parse_startup(root, &m.startup, err)) {
    return false;
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
