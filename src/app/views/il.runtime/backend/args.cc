// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/args.h"

#include "base/tuple/tuple.h"

namespace app {
namespace detail {

namespace {

using namespace base::tuple::literals;

const Value* find_named(const CallStmt& c, std::string_view named) {
  if (named.empty()) {
    return nullptr;
  }
  for (const Arg& a : c.args) {
    if (a["name"_t] == named) {
      return &a["value"_t];
    }
  }
  return nullptr;
}

const Value* find_positional(const CallStmt& c, int positional) {
  if (positional < 0) {
    return nullptr;
  }
  int pi = 0;
  for (const Arg& a : c.args) {
    if (!a["name"_t].empty()) {
      continue;
    }
    if (pi == positional) {
      return &a["value"_t];
    }
    ++pi;
  }
  return nullptr;
}

}  // namespace

int arg_int(const CallStmt& c,
            int positional,
            std::string_view named,
            int def) {
  if (const Value* v = find_named(c, named); v && is_int(*v)) {
    return as_int(*v, def);
  }
  if (const Value* v = find_positional(c, positional); v && is_int(*v)) {
    return as_int(*v, def);
  }
  return def;
}

std::string arg_ident(const CallStmt& c,
                      int positional,
                      std::string_view named,
                      std::string_view def) {
  if (const Value* v = find_named(c, named); v && is_text(*v)) {
    if (const std::string* s = as_text(*v)) {
      return *s;
    }
  }
  if (const Value* v = find_positional(c, positional); v && is_text(*v)) {
    if (const std::string* s = as_text(*v)) {
      return *s;
    }
  }
  return std::string(def);
}

std::vector<Point> arg_points(const CallStmt& c) {
  std::vector<Point> pts;
  for (const Arg& a : c.args) {
    const Value& v = a["value"_t];
    if (const Point* p = as_point(v)) {
      pts.push_back(*p);
    } else if (const std::vector<Point>* list = as_point_list(v)) {
      pts.insert(pts.end(), list->begin(), list->end());
    }
  }
  return pts;
}

std::string json_escape_path(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

std::string expand_vars(const std::string& in, const VarMap& vars) {
  std::string out;
  out.reserve(in.size() + 16);
  for (size_t i = 0; i < in.size(); ++i) {
    if (in[i] != '$' || i + 1 >= in.size()) {
      out.push_back(in[i]);
      continue;
    }
    const char n0 = in[i + 1];
    if (!((n0 >= 'a' && n0 <= 'z') || (n0 >= 'A' && n0 <= 'Z') ||
          n0 == '_')) {
      out.push_back(in[i]);
      continue;
    }
    size_t j = i + 1;
    while (j < in.size()) {
      const char c = in[j];
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_') {
        ++j;
        continue;
      }
      break;
    }
    const std::string key = in.substr(i + 1, j - i - 1);
    const auto it = vars.find(key);
    if (it == vars.end()) {
      out.push_back(in[i]);
      continue;
    }
    out += json_escape_path(it->second);
    i = j - 1;
  }
  return out;
}

bool lookup_var(const VarMap* vars, const std::string& key, std::string* out) {
  if (!vars || key.empty() || !out) {
    return false;
  }
  const auto it = vars->find(key);
  if (it == vars->end() || it->second.empty()) {
    return false;
  }
  *out = it->second;
  return true;
}

std::string resolve_ident(const std::string& raw, const VarMap* vars) {
  if (vars && raw.size() > 1 && raw[0] == '$') {
    std::string got;
    if (lookup_var(vars, raw.substr(1), &got)) {
      return got;
    }
  }
  return raw;
}

bool bind_as(VarMap* vars, const std::string& as, const std::string& path) {
  if (!vars || as.empty()) {
    return false;
  }
  (*vars)[as] = path;
  return true;
}

}  // namespace detail
}  // namespace app
