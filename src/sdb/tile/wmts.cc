// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/tile/wmts.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace sdb {
namespace tile {
namespace {

std::string to_lower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

void replace_all(std::string* s, const std::string& from,
                 const std::string& to) {
  if (s == nullptr) {
    return;
  }
  for (size_t pos = 0; (pos = s->find(from, pos)) != std::string::npos;) {
    s->replace(pos, from.size(), to);
    pos += to.size();
  }
}

size_t find_ci(const std::string& hay, const std::string& needle,
               size_t start = 0) {
  if (needle.empty() || start >= hay.size()) {
    return std::string::npos;
  }
  const std::string h = to_lower(hay.substr(start));
  const std::string n = to_lower(needle);
  const size_t pos = h.find(n);
  return pos == std::string::npos ? std::string::npos : start + pos;
}

std::string extract_attr(const std::string& tag, const char* attr) {
  auto try_quote = [&](char quote) -> std::string {
    const std::string key = std::string(attr) + "=" + quote;
    const size_t pos = find_ci(tag, key);
    if (pos == std::string::npos) {
      return {};
    }
    const size_t start = pos + key.size();
    const size_t end = tag.find(quote, start);
    if (end == std::string::npos) {
      return {};
    }
    return tag.substr(start, end - start);
  };
  std::string v = try_quote('"');
  if (!v.empty()) {
    return v;
  }
  return try_quote('\'');
}

std::string first_identifier(const std::string& xml) {
  // Prefer Layer/Identifier over Title. Scan for Identifier text nodes.
  size_t pos = 0;
  while ((pos = find_ci(xml, "Identifier", pos)) != std::string::npos) {
    const size_t gt = xml.find('>', pos);
    if (gt == std::string::npos) {
      break;
    }
    // Skip closing tags </...Identifier>
    if (pos > 0 && xml[pos - 1] == '/') {
      pos = gt + 1;
      continue;
    }
    const size_t close = xml.find('<', gt + 1);
    if (close == std::string::npos) {
      break;
    }
    std::string text = xml.substr(gt + 1, close - (gt + 1));
    while (!text.empty() &&
           std::isspace(static_cast<unsigned char>(text.front()))) {
      text.erase(text.begin());
    }
    while (!text.empty() &&
           std::isspace(static_cast<unsigned char>(text.back()))) {
      text.pop_back();
    }
    if (!text.empty()) {
      return text;
    }
    pos = close;
  }
  return {};
}

std::string extract_get_tile_href(const std::string& xml) {
  const size_t get_tile = find_ci(xml, "GetTile");
  if (get_tile == std::string::npos) {
    return {};
  }
  // Search a window after GetTile for an href attribute.
  const size_t window_end =
      (std::min)(xml.size(), get_tile + 800);
  const std::string window = xml.substr(get_tile, window_end - get_tile);
  std::string href = extract_attr(window, "xlink:href");
  if (href.empty()) {
    href = extract_attr(window, "href");
  }
  return href;
}

}  // namespace

bool normalize_wmts_url_template(const std::string& in, std::string* out) {
  if (out == nullptr || in.empty()) {
    return false;
  }
  std::string t = in;
  replace_all(&t, "{TileMatrix}", "{z}");
  replace_all(&t, "{TILEMATRIX}", "{z}");
  replace_all(&t, "{tilematrix}", "{z}");
  replace_all(&t, "{TileCol}", "{x}");
  replace_all(&t, "{TILECOL}", "{x}");
  replace_all(&t, "{tilecol}", "{x}");
  replace_all(&t, "{TileRow}", "{y}");
  replace_all(&t, "{TILEROW}", "{y}");
  replace_all(&t, "{tilerow}", "{y}");
  if (t.find("{z}") == std::string::npos ||
      t.find("{x}") == std::string::npos ||
      t.find("{y}") == std::string::npos) {
    return false;
  }
  *out = std::move(t);
  return true;
}

bool parse_wmts_capabilities(const std::string& xml, std::string* url_template,
                             std::string* error) {
  if (url_template == nullptr) {
    return false;
  }
  url_template->clear();
  auto fail = [&](const char* msg) {
    if (error) {
      *error = msg;
    }
    return false;
  };
  if (xml.empty()) {
    return fail("empty capabilities");
  }

  size_t pos = 0;
  while ((pos = find_ci(xml, "ResourceURL", pos)) != std::string::npos) {
    const size_t gt = xml.find('>', pos);
    if (gt == std::string::npos) {
      break;
    }
    const std::string tag = xml.substr(pos, gt - pos);
    const std::string resource_type = extract_attr(tag, "resourceType");
    if (!resource_type.empty() && to_lower(resource_type) != "tile") {
      pos = gt + 1;
      continue;
    }
    const std::string tmpl = extract_attr(tag, "template");
    std::string normalized;
    if (!tmpl.empty() && normalize_wmts_url_template(tmpl, &normalized)) {
      *url_template = std::move(normalized);
      if (error) {
        error->clear();
      }
      return true;
    }
    pos = gt + 1;
  }

  const std::string service_href = extract_get_tile_href(xml);
  const std::string layer = first_identifier(xml);
  if (service_href.empty() || layer.empty()) {
    return fail("no ResourceURL template and no GetTile/Layer");
  }
  std::string base = service_href;
  if (base.find('?') == std::string::npos) {
    base.push_back('?');
  } else if (base.back() != '?' && base.back() != '&') {
    base.push_back('&');
  }
  const std::string tmpl =
      base + "SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=" + layer +
      "&STYLE=default&TILEMATRIXSET=GoogleMapsCompatible"
      "&TILEMATRIX={TileMatrix}&TILECOL={TileCol}&TILEROW={TileRow}"
      "&FORMAT=image/png";
  std::string normalized;
  if (!normalize_wmts_url_template(tmpl, &normalized)) {
    return fail("failed to normalize GetTile template");
  }
  *url_template = std::move(normalized);
  if (error) {
    error->clear();
  }
  return true;
}

}  // namespace tile
}  // namespace sdb
