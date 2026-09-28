// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MARKUP_STYLE_CSS_PARSER_H_
#define UI_VIEWS_MARKUP_STYLE_CSS_PARSER_H_

#include "ui/ui_export.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ui/views/markup/style/flex_style.h"

namespace ui {
namespace views {

// Parses the v1 Flex CSS subset (tag / #id / .class; no combinators).
class UI_EXPORT CssParser {
 public:
  struct Rule {
    enum class SelectorKind {
      kTag,
      kId,
      kClass,
    };
    SelectorKind kind = SelectorKind::kTag;
    std::string name;
    std::vector<std::pair<std::string, std::string>> declarations;
  };

  bool parse(std::string_view css, std::string* error = nullptr);
  void clear();
  bool append_rules(const CssParser& other);
  const std::vector<Rule>& rules() const { return rules_; }

  // Cascade: tag → .class → #id (document order within each bucket).
  FlexStyle resolve(std::string_view tag,
                    std::string_view id,
                    const std::vector<std::string>& classes) const;

  // Upsert an #id rule with declarations (editor-owned styles).
  void upsert_id_declarations(
      std::string_view id,
      const std::vector<std::pair<std::string, std::string>>& decls);

  std::string serialize() const;

 private:
  std::vector<Rule> rules_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MARKUP_STYLE_CSS_PARSER_H_
