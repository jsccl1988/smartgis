// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MARKUP_DOCUMENT_MARKUP_DOCUMENT_H_
#define UI_VIEWS_MARKUP_DOCUMENT_MARKUP_DOCUMENT_H_

#include "ui/ui_export.h"
#include <memory>
#include <string>
#include <string_view>

#include "ui/views/markup/style/css_parser.h"

namespace pugi {
class xml_document;
}  // namespace pugi

namespace ui {
namespace views {

// Parsed .ui.xml plus optional linked / inline CSS (Flex subset).
class UI_EXPORT MarkupDocument {
 public:
  MarkupDocument();
  ~MarkupDocument();

  MarkupDocument(const MarkupDocument&) = delete;
  MarkupDocument& operator=(const MarkupDocument&) = delete;

  bool load_xml(std::string_view xml_utf8,
                std::string_view base_dir = {},
                std::string* error = nullptr);
  bool load_css(std::string_view css_utf8, std::string* error = nullptr);

  const CssParser& stylesheet() const { return css_; }
  CssParser& stylesheet() { return css_; }
  std::string name() const;

  // Mutable XML for UiDesigner edits (insert / reorder / attributes).
  pugi::xml_document* raw_xml() { return doc_.get(); }
  const pugi::xml_document* raw_xml() const { return doc_.get(); }

  std::string serialize_xml() const;
  std::string serialize_css() const;
  bool save_files(const std::string& xml_path,
                  const std::string& css_path) const;

 private:
  bool load_style_links(std::string_view base_dir, std::string* error);

  std::unique_ptr<pugi::xml_document> doc_;
  CssParser css_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MARKUP_DOCUMENT_MARKUP_DOCUMENT_H_
