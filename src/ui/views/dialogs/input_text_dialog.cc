// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/dialogs/input_text_dialog.h"

#include <memory>
#include <utility>

#include "ui/views/dialogs/dialog.h"
#include "ui/views/primitives/label.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/primitives/textfield.h"

namespace ui {
namespace views {
namespace {

class InputTextForm : public View {
 public:
  InputTextForm(const std::string& prompt, std::string* sink)
      : sink_(sink) {
    auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
    auto label = std::make_unique<Label>(prompt.empty() ? "Value" : prompt);
    label->set_preferred_size({320, 24});
    auto field = std::make_unique<Textfield>();
    field->set_preferred_size({320, 28});
    if (sink_ && !sink_->empty()) {
      field->set_text(*sink_);
    }
    field_ = field.get();
    field_->set_change([this]() { flush(); });
    set_layout_manager(std::move(box));
    add_child(std::move(label));
    add_child(std::move(field));
    set_preferred_size({340, 80});
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
