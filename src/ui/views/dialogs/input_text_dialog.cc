// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/dialogs/input_text_dialog.h"

#include <memory>
#include <utility>

#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace {

class InputTextForm : public View {
 public:
  InputTextForm(const std::string& prompt, std::string* sink) : sink_(sink) {
    MarkupRoot loaded = load_markup("toolkit/input_text.ui.xml");
    if (!loaded.ok()) {
      // Fallback empty host so Dialog still opens if resources are missing.
      set_preferred_size({340, 80});
      return;
    }
    auto* prompt_label = loaded.ids.find_as<Label>("prompt");
    field_ = loaded.ids.find_as<Textfield>("value");

    if (prompt_label) {
      prompt_label->set_text(prompt.empty() ? "Value" : prompt);
    }
    if (field_) {
      if (sink_ && !sink_->empty()) {
        field_->set_text(*sink_);
      }
      field_->set_change([this]() { flush(); });
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({340, 80});
    add_child(std::move(loaded.root));
    set_preferred_size({340, 80});
    flush();
  }

  ~InputTextForm() override { flush(); }

 private:
  void flush() {
    if (sink_ && field_) {
      *sink_ = field_->text();
    }
  }

  Textfield* field_ = nullptr;
  std::string* sink_ = nullptr;
};

}  // namespace

bool InputTextDialog::run(HWND owner, std::string* out) {
  return run(owner, L"Input Text", "Value", out);
}

bool InputTextDialog::run(HWND owner, const wchar_t* title,
                          const std::string& prompt, std::string* out) {
  std::string text = out ? *out : std::string();
  auto form = std::make_unique<InputTextForm>(prompt, &text);
  const Dialog::Result result =
      Dialog::run_modal(owner, title ? title : L"Input Text", 380, 160,
                        std::move(form));
  if (result.accepted && out) {
    *out = std::move(text);
    return true;
  }
  return result.accepted;
}

}  // namespace views
}  // namespace ui
