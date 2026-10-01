// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/text2ui/text2ui.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "ui/views/markup/document/markup_document.h"

namespace ui {
namespace views {
namespace {

std::string trim_copy(std::string_view s) {
  while (!s.empty() &&
         std::isspace(static_cast<unsigned char>(s.front()))) {
    s.remove_prefix(1);
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.remove_suffix(1);
  }
  return std::string(s);
}

std::string to_lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

bool contains_any(std::string_view hay, const std::vector<std::string_view>& needles) {
  const std::string low = to_lower(hay);
  for (std::string_view n : needles) {
    if (low.find(to_lower(n)) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// Local intent → fragment. Keep small and deterministic (YAGNI).
bool match_template(std::string_view prompt, std::string* fragment,
                    std::string* error) {
  const std::string p = trim_copy(prompt);
  if (p.empty()) {
    if (error) {
      *error = "empty prompt";
    }
    return false;
  }

  if (contains_any(p, {"empty panel", "blank panel", "空面板", "空白面板"})) {
    *fragment = R"(<vbox id="panel" class="form"/>)";
    return true;
  }
  if (contains_any(p, {"toolbar", "工具栏"})) {
    *fragment =
        R"(<hbox id="toolbar" class="row">
  <button id="tb_new" text="New"/>
  <button id="tb_open" text="Open"/>
  <button id="tb_save" text="Save"/>
</hbox>)";
    return true;
  }
  if (contains_any(p, {"button row", "buttons row", "一行按钮", "按钮行"})) {
    *fragment =
        R"(<hbox id="row" class="row">
  <button id="ok" text="OK"/>
  <button id="cancel" text="Cancel"/>
</hbox>)";
    return true;
  }
  if (contains_any(p, {"form row", "表单行", "label field", "label+field"})) {
    *fragment =
        R"(<hbox id="form_row" class="row">
  <label id="field_label" text="Name"/>
  <textfield id="field_value" text=""/>
</hbox>)";
    return true;
  }
  if (contains_any(p, {"vertical form", "竖直表单", "表单"})) {
    *fragment =
        R"(<vbox id="form" class="form">
  <label id="title" text="Form"/>
  <hbox id="row" class="row">
    <label id="name_label" text="Name"/>
    <textfield id="name" text=""/>
  </hbox>
  <hbox id="actions" class="row">
    <button id="ok" text="OK"/>
    <button id="cancel" text="Cancel"/>
  </hbox>
</vbox>)";
    return true;
  }

  if (error) {
    *error =
        "no template match; try: empty panel / toolbar / button row / form "
        "row / vertical form — or prefix with @llm ";
  }
  return false;
}

constexpr const char kLlmSystemPrompt[] =
    "You generate SmartGIS Views declarative UI markup.\n"
    "Return ONLY one XML element (fragment), no markdown, no commentary.\n"
    "Allowed tags: ui, style, vbox, hbox, view, label, button, textfield, "
    "checkbox, radiobutton, combobox, slider, scrollview, tabstrip, "
    "tableview, treeview, panel.\n"
    "Use attributes: id, text, class, and flex CSS via class when needed.\n"
    "Prefer a single root vbox or hbox with unique id values.";

}  // namespace

bool strip_llm_prefix(std::string_view prompt, std::string* rest) {
  std::string trimmed = trim_copy(prompt);
  if (trimmed.size() < 4) {
    return false;
  }
  const std::string head = to_lower(trimmed.substr(0, 4));
  if (head != "@llm") {
    return false;
  }
  std::string_view rem = trimmed;
  rem.remove_prefix(4);
  if (rest) {
    *rest = trim_copy(rem);
  }
  return true;
}

std::string extract_markup_xml(std::string_view text) {
  std::string s = trim_copy(text);
  if (s.empty()) {
    return {};
  }

  // Strip ```xml ... ``` or ``` ... ``` fences.
  if (s.rfind("```", 0) == 0) {
    const size_t first_nl = s.find('\n');
    if (first_nl != std::string::npos) {
      s = s.substr(first_nl + 1);
    }
    const size_t fence = s.rfind("```");
    if (fence != std::string::npos) {
      s = s.substr(0, fence);
    }
    s = trim_copy(s);
  }

  const size_t lt = s.find('<');
  if (lt == std::string::npos) {
    return {};
  }
  s = s.substr(lt);

  // Prefer a single root element: from first '<' to matching close or self-close.
  if (s.size() >= 2 && s[1] == '?') {
    const size_t end_decl = s.find("?>");
    if (end_decl != std::string::npos) {
      s = trim_copy(std::string_view(s).substr(end_decl + 2));
    }
  }
  if (s.empty() || s[0] != '<') {
    return {};
  }

  // If multiple top-level nodes, take through the first element's close.
  size_t i = 1;
  while (i < s.size() &&
         (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' ||
          s[i] == '-' || s[i] == ':')) {
    ++i;
  }
  const std::string tag = s.substr(1, i - 1);
  if (tag.empty()) {
    return {};
  }

  const std::string close = "</" + tag + ">";
  const size_t close_pos = s.find(close);
  if (close_pos != std::string::npos) {
    return trim_copy(s.substr(0, close_pos + close.size()));
  }
  // Self-closing root.
  const size_t gt = s.find('>');
  if (gt != std::string::npos && gt > 0 && s[gt - 1] == '/') {
    return trim_copy(s.substr(0, gt + 1));
  }
  return trim_copy(s);
}

bool validate_markup_fragment(std::string_view fragment, std::string* error) {
  const std::string frag = trim_copy(fragment);
  if (frag.empty()) {
    if (error) {
      *error = "empty markup fragment";
    }
    return false;
  }

  std::string wrapped;
  if (to_lower(frag).find("<ui") == 0) {
    wrapped = frag;
  } else {
    wrapped = std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<ui name=\"text2ui\">\n") +
              frag + "\n</ui>\n";
  }

  MarkupDocument doc;
  std::string err;
  if (!doc.load_xml(wrapped, {}, &err)) {
    if (error) {
      *error = err.empty() ? "markup parse failed" : err;
    }
    return false;
  }
  return true;
}

bool generate_text2ui(const Text2UiRequest& req, Text2UiResult* out) {
  if (!out) {
    return false;
  }
  *out = Text2UiResult{};

  std::string llm_rest;
  const bool want_llm = strip_llm_prefix(req.prompt, &llm_rest);

  std::string raw;
  if (want_llm) {
    out->used_llm = true;
    if (!req.llm) {
      out->error = "LLM requested (@llm) but no LlmBackend injected";
      return false;
    }
    if (llm_rest.empty()) {
      out->error = "empty prompt after @llm";
      return false;
    }
    std::string err;
    if (!req.llm->complete(kLlmSystemPrompt, llm_rest, &raw, &err)) {
      out->error = err.empty() ? "LLM complete failed" : err;
      return false;
    }
  } else {
    std::string err;
    if (!match_template(req.prompt, &raw, &err)) {
      out->error = err;
      return false;
    }
  }

  out->xml_fragment = extract_markup_xml(raw);
  if (out->xml_fragment.empty()) {
    out->error = "could not extract XML markup from generator output";
    return false;
  }

  std::string verr;
  if (!validate_markup_fragment(out->xml_fragment, &verr)) {
    out->error = verr;
    out->xml_fragment.clear();
    return false;
  }

  out->ok = true;
  return true;
}

}  // namespace views
}  // namespace ui
