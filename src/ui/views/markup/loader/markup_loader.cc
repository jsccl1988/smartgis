// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/loader/markup_loader.h"

#include <cctype>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "pugixml.hpp"
#include "base/trace/event/process_trace.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/markup/factory/placeholder_view.h"
#include "ui/views/markup/layout/yoga_layout_manager.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {

MarkupRoot::MarkupRoot() = default;
MarkupRoot::MarkupRoot(MarkupRoot&&) noexcept = default;
MarkupRoot& MarkupRoot::operator=(MarkupRoot&&) noexcept = default;
MarkupRoot::~MarkupRoot() = default;

bool MarkupRoot::ok() const {
  return root != nullptr;
}

namespace {

std::string to_lower(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    out.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

std::vector<std::string> split_classes(std::string_view raw) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : raw) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) {
    out.push_back(cur);
  }
  return out;
}

bool is_flex_container_tag(std::string_view tag) {
  const std::string t = to_lower(tag);
  // Plain "view" is a generic node — only attach Yoga when it has children.
  return t == "vbox" || t == "hbox";
}

FlexStyle sugar_direction(std::string_view tag, FlexStyle style) {
  const std::string t = to_lower(tag);
  if (t == "hbox") {
    style.flex_direction = FlexStyle::FlexDirection::kRow;
    style.display = FlexStyle::Display::kFlex;
  } else if (t == "vbox") {
    style.flex_direction = FlexStyle::FlexDirection::kColumn;
    style.display = FlexStyle::Display::kFlex;
  }
  return style;
}

void apply_paint_hints(View* view, const FlexStyle& style) {
  if (auto* label = dynamic_cast<Label*>(view)) {
    if (style.color.has_value()) {
      label->set_color(*style.color);
    }
  }
}

MarkupAttrs attrs_from_node(const pugi::xml_node& node) {
  MarkupAttrs attrs;
  for (pugi::xml_attribute a : node.attributes()) {
    attrs.values.emplace(a.name(), a.value());
  }
  return attrs;
}

pugi::xml_node content_root(const pugi::xml_document& doc) {
  pugi::xml_node ui = doc.child("ui");
  if (!ui) {
    ui = doc.document_element();
  }
  if (!ui) {
    return {};
  }
  for (pugi::xml_node child : ui.children()) {
    if (child.type() != pugi::node_element) {
      continue;
    }
    if (std::string_view(child.name()) == "style") {
      continue;
    }
    return child;
  }
  return {};
}

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

std::string wide_to_utf8_path(const std::wstring& w) {
  if (w.empty()) {
    return {};
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0,
                                    nullptr, nullptr);
  if (n <= 0) {
    return {};
  }
  std::string u8(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, u8.data(), n, nullptr,
                      nullptr);
  u8.resize(static_cast<size_t>(n - 1));
  return u8;
}

std::string read_file(const std::string& path) {
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

bool file_exists(const std::string& path) {
  const std::wstring wpath = utf8_to_wide_path(path);
  if (wpath.empty()) {
    return false;
  }
  const DWORD attr = GetFileAttributesW(wpath.c_str());
  return attr != INVALID_FILE_ATTRIBUTES &&
         !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::string exe_dir() {
  wchar_t buf[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  std::wstring path(buf, n);
  const auto slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos) {
    return {};
  }
  return wide_to_utf8_path(path.substr(0, slash));
}

std::string dirname_of(const std::string& path) {
  const auto slash = path.find_last_of("\\/");
  if (slash == std::string::npos) {
    return {};
  }
  return path.substr(0, slash);
}

std::string resolve_markup_path_impl(std::string_view path_or_name) {
  const std::string name(path_or_name);
  if (name.empty()) {
    return {};
  }
  if (file_exists(name)) {
    return name;
  }
  const std::string exe = exe_dir();
  // Prefer shared out/ui/ (exe is under out/Debug|Release → ../ui), same as
  // out/data/. Keep <exe>/ui/ for packaged layouts that ship beside the binary.
  const std::string candidates[] = {
      exe + "\\..\\ui\\" + name,
      exe + "/../ui/" + name,
      exe + "\\ui\\" + name,
      exe + "/ui/" + name,
      exe + "/" + name,
      std::string("ui\\") + name,
      std::string("ui/") + name,
      std::string("..\\ui\\") + name,
      std::string("../ui/") + name,
      std::string("src\\ui\\resources\\") + name,
      std::string("src/ui/resources/") + name,
      std::string("src\\ui\\views\\markup\\testdata\\") + name,
      std::string("src/ui/views/markup/testdata/") + name,
  };
  for (const std::string& c : candidates) {
    if (file_exists(c)) {
      return c;
    }
  }
  return {};
}

void apply_combobox_items(Combobox* combo, const pugi::xml_node& node) {
  if (!combo) {
    return;
  }
  for (pugi::xml_node child : node.children("item")) {
    const char* text = child.attribute("text").as_string("");
    combo->add_item(text);
  }
}

std::unique_ptr<View> build_node(const pugi::xml_node& node,
                                 const CssParser& css,
                                 const ControlFactory& factory,
                                 NamedViewMap* ids,
                                 std::string* error) {
  const MarkupAttrs attrs = attrs_from_node(node);
  std::unique_ptr<View> view = factory.create(node.name(), attrs);
  if (!view) {
    // Unknown tag → placeholder stub (GIS / future controls).
    const std::string caption =
        attrs.get("id").empty() ? std::string(node.name()) : attrs.get("id");
    view = std::make_unique<PlaceholderView>(caption);
  }

  if (auto* combo = dynamic_cast<Combobox*>(view.get())) {
    apply_combobox_items(combo, node);
  }

  const std::string id = attrs.get("id");
  const auto classes = split_classes(attrs.get("class"));
  FlexStyle style = css.resolve(node.name(), id, classes);
  style = sugar_direction(node.name(), style);
  apply_paint_hints(view.get(), style);

  if (style.width.has_value() || style.height.has_value()) {
    Size pref = view->preferred_size();
    if (style.width.has_value()) {
      pref.width = static_cast<int>(*style.width);
    }
    if (style.height.has_value()) {
      pref.height = static_cast<int>(*style.height);
    }
    view->set_preferred_size(pref);
  }

  if (!id.empty()) {
    ids->put(id, view.get());
  }

  std::vector<pugi::xml_node> element_children;
  for (pugi::xml_node child : node.children()) {
    if (child.type() != pugi::node_element) {
      continue;
    }
    if (std::string_view(child.name()) == "item" ||
        std::string_view(child.name()) == "style") {
      continue;
    }
    element_children.push_back(child);
  }

  // Splitter owns pane layout (drag bar). Nest children without Yoga so the
  // Splitter::layout override is not shadowed by a LayoutManager.
  if (to_lower(node.name()) == "splitter") {
    for (const pugi::xml_node& child_xml : element_children) {
      auto child_view = build_node(child_xml, css, factory, ids, error);
      if (!child_view) {
        return nullptr;
      }
      view->add_child(std::move(child_view));
    }
    return view;
  }

  if (!element_children.empty() || is_flex_container_tag(node.name())) {
    auto yoga = std::make_unique<YogaLayoutManager>();
    yoga->set_host_style(style);
    for (const pugi::xml_node& child_xml : element_children) {
      auto child_view = build_node(child_xml, css, factory, ids, error);
      if (!child_view) {
        return nullptr;
      }
      MarkupAttrs child_attrs = attrs_from_node(child_xml);
      FlexStyle child_style =
          css.resolve(child_xml.name(), child_attrs.get("id"),
                      split_classes(child_attrs.get("class")));
      child_style = sugar_direction(child_xml.name(), child_style);
      View* raw = child_view.get();
      yoga->set_child_style(raw, child_style);
      view->add_child(std::move(child_view));
    }
    view->set_layout_manager(std::move(yoga));
  }

  return view;
}

}  // namespace

std::string resolve_markup_path(std::string_view path_or_name) {
  return resolve_markup_path_impl(path_or_name);
}

bool build_markup_tree(const MarkupDocument& doc,
                       const MarkupOptions& options,
                       MarkupRoot* out) {
  if (!out) {
    return false;
  }
  out->root.reset();
  out->ids.clear();
  out->error.clear();
  out->name = doc.name();

  if (!doc.raw_xml()) {
    out->error = "markup: null document";
    return false;
  }
  pugi::xml_node root_xml = content_root(*doc.raw_xml());
  if (!root_xml) {
    out->error = "markup: no content root";
    return false;
  }

  ControlFactory owned;
  const ControlFactory* factory = options.factory;
  if (!factory) {
    owned = ControlFactory::make_default();
    factory = &owned;
  }

  std::string err;
  out->root =
      build_node(root_xml, doc.stylesheet(), *factory, &out->ids, &err);
  if (!out->root) {
    out->error = err.empty() ? "markup: build failed" : err;
    return false;
  }
  return true;
}

bool load_markup_bytes(std::string_view xml_utf8,
                       std::string_view base_dir,
                       const MarkupOptions& options,
                       MarkupRoot* out) {
  if (!out) {
    return false;
  }
  MarkupDocument doc;
  std::string err;
  if (!doc.load_xml(xml_utf8, base_dir, &err)) {
    out->root.reset();
    out->error = err;
    return false;
  }
  return build_markup_tree(doc, options, out);
}

MarkupRoot load_markup(std::string_view path_or_name,
                       const MarkupOptions& options) {
  BASE_TRACE_EVENT("LoadMarkup", "startup");
  MarkupRoot out;
  const std::string path = resolve_markup_path_impl(path_or_name);
  if (path.empty()) {
    out.error = std::string("markup: not found: ") + std::string(path_or_name);
    return out;
  }
  // Process-wide path→XML cache: same catalog/panel markup is often loaded
  // once per panel construction; skip redundant disk reads.
  static std::mutex cache_mu;
  static std::unordered_map<std::string, std::string> xml_cache;
  std::string xml;
  {
    std::lock_guard<std::mutex> lock(cache_mu);
    auto it = xml_cache.find(path);
    if (it != xml_cache.end()) {
      xml = it->second;
    }
  }
  if (xml.empty()) {
    xml = read_file(path);
    if (xml.empty()) {
      out.error = "markup: empty file: " + path;
      return out;
    }
    std::lock_guard<std::mutex> lock(cache_mu);
    xml_cache.emplace(path, xml);
  }
  const std::string base = dirname_of(path);
  if (!load_markup_bytes(xml, base, options, &out)) {
    if (out.error.empty()) {
      out.error = "markup: load failed";
    }
  }
  return out;
}

}  // namespace views
}  // namespace ui
