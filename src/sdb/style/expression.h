// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_STYLE_EXPRESSION_H_
#define SDB_STYLE_EXPRESSION_H_

#include "sdb/gis_export.h"
#include "sdb/style/style_types.h"

#include <cstdlib>
#include <sstream>
#include <string>

namespace sdb {
namespace style {

// Constant result of MapLibre-subset expression evaluation (v1+).
struct ExprValue {
  enum class Kind {
    kNull = 0,
    kBool,
    kNumber,
    kString,
  };

  Kind kind = Kind::kNull;
  bool b = false;
  double n = 0;
  std::string s;

  std::string as_string() const {
    switch (kind) {
      case Kind::kString:
        return s;
      case Kind::kNumber: {
        std::ostringstream os;
        os << n;
        return os.str();
      }
      case Kind::kBool:
        return b ? "true" : "false";
      default:
        return std::string();
    }
  }

  double as_number(bool* ok = nullptr) const {
    if (kind == Kind::kNumber) {
      if (ok) {
        *ok = true;
      }
      return n;
    }
    if (kind == Kind::kString) {
      char* stop = nullptr;
      double v = std::strtod(s.c_str(), &stop);
      if (ok) {
        *ok = stop != s.c_str();
      }
      return v;
    }
    if (kind == Kind::kBool) {
      if (ok) {
        *ok = true;
      }
      return b ? 1.0 : 0.0;
    }
    if (ok) {
      *ok = false;
    }
    return 0;
  }

  bool as_bool(bool* ok = nullptr) const {
    if (kind == Kind::kBool) {
      if (ok) {
        *ok = true;
      }
      return b;
    }
    if (kind == Kind::kNumber) {
      if (ok) {
        *ok = true;
      }
      return n != 0;
    }
    if (kind == Kind::kString) {
      if (ok) {
        *ok = true;
      }
      return s == "true" || s == "1";
    }
    if (ok) {
      *ok = false;
    }
    return false;
  }
};

// Evaluate a JSON array expression (or bare literal) to a constant.
// Supported ops: get, literal, zoom, ==, !=, <, <=, >, >=.
// Nested arrays are evaluated recursively. Uses AttrMap + zoom.
GIS_EXPORT bool eval_expression(const std::string& json,
                                const AttrMap& attrs,
                                double zoom,
                                ExprValue* out);

// True when `raw` looks like a JSON array expression (first element is an op).
GIS_EXPORT bool looks_like_expression(const std::string& raw);

}  // namespace style
}  // namespace sdb

#endif  // SDB_STYLE_EXPRESSION_H_
