// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_UI_DESIGNER_DOCUMENT_EDIT_H_
#define APP_UI_DESIGNER_DOCUMENT_EDIT_H_

#include <string>

#include "ui/views/markup/document/markup_document.h"

namespace app {

// Outcome of a sibling drag-drop reorder attempt.
enum class DropReorderResult {
  kOk,
  kNoop,
  kNotSibling,
  kMissing,
};

// Inserts or replaces markup from a text2ui XML fragment. On failure writes
// a short English message to |error| (when non-null) and returns false.
bool apply_markup_fragment(ui::views::MarkupDocument& document,
                           const std::string& selected_id,
                           const std::string& fragment,
                           bool replace,
                           std::string* error);

// Moves |selected_id| among element siblings by |delta| (-1 up, +1 down).
// Returns false when there is nothing to do.
bool reorder_xml_sibling(ui::views::MarkupDocument& document,
                         const std::string& selected_id,
                         int delta);

// Reorders |moving| among siblings to before |before_id|, or append when
// |append| is true.
DropReorderResult apply_drop_reorder_xml(ui::views::MarkupDocument& document,
                                         const std::string& moving,
                                         const std::string& before_id,
                                         bool append);

}  // namespace app

#endif  // APP_UI_DESIGNER_DOCUMENT_EDIT_H_
