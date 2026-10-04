// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/document/markup_document.h"

#include <cstdio>
#include <sstream>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "pugixml.hpp"

namespace ui {
namespace views {
namespace {

std::wstring utf8_to_wide_path(const std::string& u8) {
  if (u8.empty()) {
    return {};
  }
  const int n =
      MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
  if (n <= 0) {
    return {};
  }
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), n);
  w.resize(static_cast<size_t>(n - 1));
  return w;
}

std::string read_file_utf8(const std::string& path) {
  const std::wstring wpath = utf8_to_wide_path(path);
  if (wpath.empty()) {
    return {};
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, wpath.c_str(), L"rb") != 0 || !f) {
    return {};
  }
  std::ostringstream ss;
  char buf[4096];
  while (const size_t n = fread(buf, 1, sizeof(buf), f)) {
    ss.write(buf, static_cast<std::streamsize>(n));
  }
  fclose(f);
  return ss.str();
}

bool write_file_utf8(const std::string& path, const std::string& data) {
  const std::wstring wpath = utf8_to_wide_path(path);
  if (wpath.empty()) {
    return false;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, wpath.c_str(), L"wb") != 0 || !f) {
    return false;
  }
  const size_t n =
      fwrite(data.data(), 1, data.size(), f);
  fclose(f);
  return n == data.size();
}

std::string join_path(std::string_view base_dir, std::string_view rel) {
  if (base_dir.empty()) {
    return std::string(rel);
  }
  std::string out(base_dir);
  if (!out.empty() && out.back() != '/' && out.back() != '\\') {
    out.push_back('/');
  }
  out.append(rel);
  return out;
}

}  // namespace

MarkupDocument::MarkupDocument()
    : doc_(std::make_unique<pugi::xml_document>()) {}

MarkupDocument::~MarkupDocument() = default;

bool MarkupDocument::load_xml(std::string_view xml_utf8,
                              std::string_view base_dir,
                              std::string* error) {
  doc_->reset();
  css_.clear();
  const pugi::xml_parse_result r =
      doc_->load_buffer(xml_utf8.data(), xml_utf8.size());
  if (!r) {
    if (error) {
      *error = std::string("xml: ") + r.description();
    }
    return false;
  }
  return load_style_links(base_dir, error);
}

bool MarkupDocument::load_css(std::string_view css_utf8, std::string* error) {
  CssParser extra;
  if (!extra.parse(css_utf8, error)) {
    return false;
  }
  return css_.append_rules(extra);
}

bool MarkupDocument::load_style_links(std::string_view base_dir,
                                      std::string* error) {
  pugi::xml_node ui = doc_->child("ui");
  if (!ui) {
    ui = doc_->document_element();
  }
  if (!ui) {
    if (error) {
      *error = "xml: missing root";
    }
    return false;
  }

  for (pugi::xml_node child : ui.children()) {
    if (std::string_view(child.name()) != "style") {
      continue;
    }
    if (const char* src = child.attribute("src").as_string(nullptr)) {
      const std::string path = join_path(base_dir, src);
      const std::string text = read_file_utf8(path);
      if (text.empty()) {
        if (error) {
          *error = "css: failed to read " + path;
        }
        return false;
      }
      if (!load_css(text, error)) {
        return false;
      }
    } else if (child.child_value() && child.child_value()[0] != '\0') {
      if (!load_css(child.child_value(), error)) {
        return false;
      }
    }
  }
  return true;
}

std::string MarkupDocument::name() const {
  pugi::xml_node ui = doc_->child("ui");
  if (!ui) {
    ui = doc_->document_element();
  }
  if (!ui) {
    return {};
  }
  return ui.attribute("name").as_string("");
}

std::string MarkupDocument::serialize_xml() const {
  std::ostringstream ss;
  doc_->save(ss, "  ");
  return ss.str();
}

std::string MarkupDocument::serialize_css() const {
  return css_.serialize();
}

bool MarkupDocument::save_files(const std::string& xml_path,
                                const std::string& css_path) const {
  if (!write_file_utf8(xml_path, serialize_xml())) {
    return false;
  }
  if (!css_path.empty() && !write_file_utf8(css_path, serialize_css())) {
    return false;
  }
  return true;
}

}  // namespace views
}  // namespace ui
