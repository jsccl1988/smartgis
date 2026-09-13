// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/create_map_dialog.h"

#include <memory>
#include <utility>

#include "ui/views/dialog.h"
#include "ui/views/label.h"
#include "ui/views/layout.h"
#include "ui/views/textfield.h"

namespace ui {
namespace views {
namespace {

class CreateMapForm : public View {
 public:
  explicit CreateMapForm(std::string* sink) : sink_(sink) {
    auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
    auto label = std::make_unique<Label>("Map name");
    label->set_preferred_size({320, 22});
    auto name = std::make_unique<Textfield>();
    name->set_preferred_size({320, 28});
    if (sink_ && !sink_->empty()) {
      name->set_text(*sink_);
    }
    name_ = name.get();
    name_->set_change([this]() { flush(); });
    set_layout_manager(std::move(box));
    add_child(std::move(label));
    add_child(std::move(name));
    set_preferred_size({340, 70});
  }

  ~CreateMapForm() override { flush(); }

 private:
  void flush() {
    if (sink_ && name_) {
      *sink_ = name_->text();
    }
  }

  Textfield* name_ = nullptr;
  std::string* sink_ = nullptr;
};

}  // namespace

bool CreateMapDialog::run(HWND owner, std::string* name) {
  std::string local = name ? *name : std::string();
  auto form = std::make_unique<CreateMapForm>(&local);
  const Dialog::Result result =
      Dialog::run_modal(owner, L"Create Map", 380, 160, std::move(form));
  if (result.accepted && name) {
    *name = std::move(local);
    return true;
  }
  return result.accepted;
}

}  // namespace views
}  // namespace ui
