// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_TEXT2UI_TEXT2UI_H_
#define UI_VIEWS_TEXT2UI_TEXT2UI_H_

#include "ui/ui_export.h"
#include <string>
#include <string_view>

#include "ui/views/text2ui/llm_backend.h"

namespace ui {
namespace views {

// Request for natural-language → Views markup fragment generation.
struct UI_EXPORT Text2UiRequest {
  std::string prompt;
  // Required when prompt starts with "@llm" (after trim). Unused for templates.
  LlmBackend* llm = nullptr;
};

// Generated single-root markup fragment (e.g. <vbox id="…">…</vbox>).
struct UI_EXPORT Text2UiResult {
  bool ok = false;
  bool used_llm = false;
  std::string xml_fragment;
  std::string error;
};

// True when |prompt| begins with "@llm" (case-insensitive, optional space).
// Writes the remainder into |rest| (trimmed).
UI_EXPORT bool strip_llm_prefix(std::string_view prompt, std::string* rest);

// Pull the first XML root element from model/template text (strips ``` fences).
UI_EXPORT std::string extract_markup_xml(std::string_view text);

// Wrap fragment under <ui> and parse via MarkupDocument. Rejects empty/bad XML.
UI_EXPORT bool validate_markup_fragment(std::string_view fragment,
                                        std::string* error);

// Template by default; "@llm …" routes to |req.llm|. Always validates before OK.
UI_EXPORT bool generate_text2ui(const Text2UiRequest& req, Text2UiResult* out);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_TEXT2UI_TEXT2UI_H_
