// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/style/css_parser.h"

#include <cctype>
#include <cstdlib>
#include <unordered_map>

namespace ui {
namespace views {
namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() &&
         std::isspace(static_cast<unsigned char>(s.front()))) {
    s.remove_prefix(1);
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.remove_suffix(1);
  }
  return s;
}

std::string to_lower(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    out.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

bool parse_float(std::string_view s, float* out) {
  if (!out || s.empty()) {
    return false;
  }
  const std::string tmp(s);
  char* end = nullptr;
  const float v = std::strtof(tmp.c_str(), &end);
  if (end == tmp.c_str()) {
    return false;
  }
  *out = v;
  return true;
}

bool parse_px(std::string_view raw, float* out) {
  raw = trim(raw);
  const std::string lower = to_lower(raw);
  std::string_view num = lower;
  // Reject percentages / other units — strtof("100%") would otherwise yield 100.
  if (!num.empty() && num.back() == '%') {
    return false;
  }
  if (num.size() >= 2 && num.substr(num.size() - 2) == "px") {
    num = num.substr(0, num.size() - 2);
  } else if (!num.empty() && std::isalpha(static_cast<unsigned char>(num.back()))) {
    return false;
  }
  return parse_float(num, out);
}

bool parse_hex_byte(char c, int* n) {
  if (c >= '0' && c <= '9') {
    *n = c - '0';
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    *n = 10 + (c - 'a');
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    *n = 10 + (c - 'A');
    return true;
  }
  return false;
}

bool parse_color(std::string_view raw, ui::gfx::Color* out) {
  if (!out) {
    return false;
  }
  raw = trim(raw);
  if (!raw.empty() && raw.front() == '#') {
    raw.remove_prefix(1);
  }
  if (raw.size() == 6) {
    int r1, r2, g1, g2, b1, b2;
    if (!parse_hex_byte(raw[0], &r1) || !parse_hex_byte(raw[1], &r2) ||
        !parse_hex_byte(raw[2], &g1) || !parse_hex_byte(raw[3], &g2) ||
        !parse_hex_byte(raw[4], &b1) || !parse_hex_byte(raw[5], &b2)) {
      return false;
    }
    *out = ui::gfx::color_rgb(r1 * 16 + r2, g1 * 16 + g2, b1 * 16 + b2);
    return true;
  }
  if (raw.size() == 3) {
    int r, g, b;
    if (!parse_hex_byte(raw[0], &r) || !parse_hex_byte(raw[1], &g) ||
        !parse_hex_byte(raw[2], &b)) {
      return false;
    }
    *out = ui::gfx::color_rgb(r * 17, g * 17, b * 17);
    return true;
  }
  return false;
}

void apply_declaration(std::string_view prop,
                       std::string_view value,
                       FlexStyle* style) {
  if (!style) {
    return;
  }
  const std::string p = to_lower(trim(prop));
  value = trim(value);
  const std::string v = to_lower(value);
  float f = 0.f;

  if (p == "display") {
    if (v == "none") {
      style->display = FlexStyle::Display::kNone;
    } else if (v == "flex") {
      style->display = FlexStyle::Display::kFlex;
    }
    return;
  }
  if (p == "flex-direction") {
    if (v == "row") {
      style->flex_direction = FlexStyle::FlexDirection::kRow;
    } else if (v == "column") {
      style->flex_direction = FlexStyle::FlexDirection::kColumn;
    }
    return;
  }
  if (p == "justify-content") {
    using J = FlexStyle::Justify;
    static const std::unordered_map<std::string, J> kMap = {
        {"flex-start", J::kFlexStart},
        {"flex-end", J::kFlexEnd},
        {"center", J::kCenter},
        {"space-between", J::kSpaceBetween},
        {"space-around", J::kSpaceAround},
        {"space-evenly", J::kSpaceEvenly},
    };
    const auto it = kMap.find(v);
    if (it != kMap.end()) {
      style->justify_content = it->second;
    }
    return;
  }
  if (p == "align-items") {
    using A = FlexStyle::Align;
    static const std::unordered_map<std::string, A> kMap = {
        {"auto", A::kAuto},
        {"flex-start", A::kFlexStart},
        {"flex-end", A::kFlexEnd},
        {"center", A::kCenter},
        {"stretch", A::kStretch},
    };
    const auto it = kMap.find(v);
    if (it != kMap.end()) {
      style->align_items = it->second;
    }
    return;
  }
  if (p == "flex-grow" && parse_float(v, &f)) {
    style->flex_grow = f;
    return;
  }
  if (p == "flex" && parse_float(v.substr(0, v.find(' ')), &f)) {
    style->flex = f;
    style->flex_grow = f;
    return;
  }
  if (p == "gap" && parse_px(value, &f)) {
    style->gap = f;
    return;
  }
  if (p == "padding" && parse_px(value, &f)) {
    style->padding = f;
    return;
  }
  if (p == "margin" && parse_px(value, &f)) {
    style->margin = f;
    return;
  }
  if (p == "width" && parse_px(value, &f)) {
    style->width = f;
    return;
  }
  if (p == "height" && parse_px(value, &f)) {
    style->height = f;
    return;
  }
  if (p == "min-width" && parse_px(value, &f)) {
    style->min_width = f;
    return;
  }
  if (p == "min-height" && parse_px(value, &f)) {
    style->min_height = f;
    return;
  }
  if (p == "max-width" && parse_px(value, &f)) {
    style->max_width = f;
    return;
  }
  if (p == "max-height" && parse_px(value, &f)) {
    style->max_height = f;
    return;
  }
  if (p == "font-size" && parse_px(value, &f)) {
    style->font_size = f;
    return;
  }
  if (p == "color") {
    ui::gfx::Color c = 0;
    if (parse_color(value, &c)) {
      style->color = c;
    }
    return;
  }
  if (p == "background-color") {
    ui::gfx::Color c = 0;
    if (parse_color(value, &c)) {
      style->background_color = c;
    }
  }
}

void apply_rule(const CssParser::Rule& rule, FlexStyle* style) {
  for (const auto& decl : rule.declarations) {
    apply_declaration(decl.first, decl.second, style);
  }
}

}  // namespace

bool CssParser::parse(std::string_view css, std::string* error) {
  rules_.clear();
  size_t i = 0;
  const size_t n = css.size();
  while (i < n) {
    while (i < n && std::isspace(static_cast<unsigned char>(css[i]))) {
      ++i;
    }
    if (i >= n) {
      break;
    }
    if (i + 1 < n && css[i] == '/' && css[i + 1] == '*') {
      i += 2;
      while (i + 1 < n && !(css[i] == '*' && css[i + 1] == '/')) {
        ++i;
      }
      if (i + 1 < n) {
        i += 2;
      }
      continue;
    }

    const size_t sel_begin = i;
    while (i < n && css[i] != '{') {
      ++i;
    }
    if (i >= n) {
      if (error) {
        *error = "css: missing '{'";
      }
      return false;
    }
    std::string_view selector = trim(css.substr(sel_begin, i - sel_begin));
    ++i;
    const size_t body_begin = i;
    while (i < n && css[i] != '}') {
      ++i;
    }
    if (i >= n) {
      if (error) {
        *error = "css: missing '}'";
      }
      return false;
    }
    std::string_view body = css.substr(body_begin, i - body_begin);
    ++i;
    if (selector.empty()) {
      continue;
    }

    // Support comma-separated selectors sharing one body.
    size_t sel_i = 0;
    while (sel_i < selector.size()) {
      while (sel_i < selector.size() &&
             std::isspace(static_cast<unsigned char>(selector[sel_i]))) {
        ++sel_i;
      }
      const size_t one_begin = sel_i;
      while (sel_i < selector.size() && selector[sel_i] != ',') {
        ++sel_i;
      }
      std::string_view one = trim(selector.substr(one_begin, sel_i - one_begin));
      if (sel_i < selector.size()) {
        ++sel_i;
      }
      if (one.empty()) {
        continue;
      }

      Rule rule;
      if (one.front() == '#') {
        rule.kind = Rule::SelectorKind::kId;
        rule.name = std::string(one.substr(1));
      } else if (one.front() == '.') {
        rule.kind = Rule::SelectorKind::kClass;
        rule.name = std::string(one.substr(1));
      } else {
        rule.kind = Rule::SelectorKind::kTag;
        rule.name = to_lower(one);
      }

      size_t j = 0;
      while (j < body.size()) {
        while (j < body.size() &&
               std::isspace(static_cast<unsigned char>(body[j]))) {
          ++j;
        }
        if (j >= body.size()) {
          break;
        }
        const size_t prop_begin = j;
        while (j < body.size() && body[j] != ':' && body[j] != ';') {
          ++j;
        }
        if (j >= body.size() || body[j] != ':') {
          while (j < body.size() && body[j] != ';') {
            ++j;
          }
          if (j < body.size()) {
            ++j;
          }
          continue;
        }
        std::string_view prop = trim(body.substr(prop_begin, j - prop_begin));
        ++j;
        const size_t val_begin = j;
        while (j < body.size() && body[j] != ';') {
          ++j;
        }
        std::string_view val = trim(body.substr(val_begin, j - val_begin));
        if (j < body.size()) {
          ++j;
        }
        if (!prop.empty()) {
          rule.declarations.emplace_back(std::string(prop), std::string(val));
        }
      }
      rules_.push_back(std::move(rule));
    }
  }
  return true;
}

void CssParser::clear() {
  rules_.clear();
}

bool CssParser::append_rules(const CssParser& other) {
  rules_.insert(rules_.end(), other.rules_.begin(), other.rules_.end());
  return true;
}

FlexStyle CssParser::resolve(std::string_view tag,
                             std::string_view id,
                             const std::vector<std::string>& classes) const {
  FlexStyle out;
  const std::string tag_l = to_lower(tag);
  for (const Rule& r : rules_) {
    if (r.kind == Rule::SelectorKind::kTag && r.name == tag_l) {
      apply_rule(r, &out);
    }
  }
  for (const Rule& r : rules_) {
    if (r.kind != Rule::SelectorKind::kClass) {
      continue;
    }
    for (const std::string& c : classes) {
      if (c == r.name) {
        apply_rule(r, &out);
        break;
      }
    }
  }
  if (!id.empty()) {
    for (const Rule& r : rules_) {
      if (r.kind == Rule::SelectorKind::kId && r.name == id) {
        apply_rule(r, &out);
      }
    }
  }
  return out;
}

void CssParser::upsert_id_declarations(
    std::string_view id,
    const std::vector<std::pair<std::string, std::string>>& decls) {
  if (id.empty()) {
    return;
  }
  const std::string id_l = to_lower(id);
  for (Rule& r : rules_) {
    if (r.kind == Rule::SelectorKind::kId && r.name == id_l) {
      for (const auto& d : decls) {
        bool replaced = false;
        for (auto& existing : r.declarations) {
          if (existing.first == d.first) {
            existing.second = d.second;
            replaced = true;
            break;
          }
        }
        if (!replaced) {
          r.declarations.push_back(d);
        }
      }
      return;
    }
  }
  Rule rule;
  rule.kind = Rule::SelectorKind::kId;
  rule.name = id_l;
  rule.declarations = decls;
  rules_.push_back(std::move(rule));
}

std::string CssParser::serialize() const {
  std::string out;
  for (const Rule& r : rules_) {
    if (r.kind == Rule::SelectorKind::kId) {
      out += '#';
    } else if (r.kind == Rule::SelectorKind::kClass) {
      out += '.';
    }
    out += r.name;
    out += " {\n";
    for (const auto& d : r.declarations) {
      out += "  ";
      out += d.first;
      out += ": ";
      out += d.second;
      out += ";\n";
    }
    out += "}\n\n";
  }
  return out;
}

}  // namespace views
}  // namespace ui
