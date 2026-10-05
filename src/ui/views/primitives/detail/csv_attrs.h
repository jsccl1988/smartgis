// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_DETAIL_CSV_ATTRS_H_
#define UI_VIEWS_PRIMITIVES_DETAIL_CSV_ATTRS_H_

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace ui {
namespace views {
namespace detail {

inline void trim_ascii(std::string* s) {
  if (!s) {
    return;
  }
  while (!s->empty() && std::isspace(static_cast<unsigned char>(s->front()))) {
    s->erase(s->begin());
  }
  while (!s->empty() && std::isspace(static_cast<unsigned char>(s->back()))) {
    s->pop_back();
  }
}

inline std::vector<std::string> split_csv(std::string_view raw) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : raw) {
    if (c == ',') {
      trim_ascii(&cur);
      if (!cur.empty()) {
        out.push_back(cur);
      }
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  trim_ascii(&cur);
  if (!cur.empty()) {
    out.push_back(cur);
  }
  return out;
}

inline bool attr_is_true(std::string_view raw) {
  return raw == "true" || raw == "1" || raw == "yes";
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_DETAIL_CSV_ATTRS_H_
