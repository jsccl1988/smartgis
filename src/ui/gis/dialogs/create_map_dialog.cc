// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/dialogs/create_map_dialog.h"

#include <memory>
#include <utility>

#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace {

class CreateMapForm : public View {
 public:
  explicit CreateMapForm(std::string* sink) : sink_(sink) {
    MarkupRoot loaded = load_markup("dialogs/create_map.ui.xml");
    if (!loaded.ok()) {
      set_preferred_size({340, 70});
      return;
    }
    name_ = loaded.ids.find_as<Textfield>("name");

    if (name_) {
      if (sink_ && !sink_->empty()) {
        name_->set_text(*sink_);
      }
      name_->set_change([this]() { flush(); });
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({340, 70});
    add_child(std::move(loaded.root));
    set_preferred_size({340, 70});
    flush();
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
