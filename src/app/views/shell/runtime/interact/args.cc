// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/interact/args.h"

namespace app {
namespace detail {

int arg_int(const CallStmt& c, int positional, const char* named, int def) {
  if (named) {
    for (const Arg& a : c.args) {
      if (a.name == named && a.value.kind == Value::Kind::kInt) {
        return a.value.ival;
      }
    }
  }
  if (positional < 0) {
    return def;
  }
  size_t pi = 0;
  for (const Arg& a : c.args) {
    if (!a.name.empty()) {
      continue;
    }
    if (static_cast<int>(pi) == positional && a.value.kind == Value::Kind::kInt) {
      return a.value.ival;
    }
    ++pi;
  }
  return def;
}

std::string arg_ident(const CallStmt& c,
                      int positional,
                      const char* named,
                      const char* def) {
  if (named) {
    for (const Arg& a : c.args) {
      if (a.name == named &&
          (a.value.kind == Value::Kind::kIdent ||
           a.value.kind == Value::Kind::kString)) {
        return a.value.sval;
      }
    }
  }
  if (positional < 0) {
    return def ? def : "";
  }
  size_t pi = 0;
  for (const Arg& a : c.args) {
    if (!a.name.empty()) {
      continue;
    }
    if (static_cast<int>(pi) == positional &&
        (a.value.kind == Value::Kind::kIdent ||
         a.value.kind == Value::Kind::kString)) {
      return a.value.sval;
    }
    ++pi;
  }
  return def ? def : "";
}

std::vector<Point> arg_points(const CallStmt& c) {
  std::vector<Point> pts;
  for (const Arg& a : c.args) {
    if (a.value.kind == Value::Kind::kPoint) {
      pts.push_back(a.value.point);
    } else if (a.value.kind == Value::Kind::kPointList) {
      pts.insert(pts.end(), a.value.points.begin(), a.value.points.end());
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

bool bind_as(VarMap* vars, const CallStmt& c, const std::string& path) {
  if (!vars) {
    return false;
  }
  const std::string as = arg_ident(c, -1, "as", "");
  if (as.empty()) {
    return false;
  }
  (*vars)[as] = path;
  return true;
}

}  // namespace detail
}  // namespace app
