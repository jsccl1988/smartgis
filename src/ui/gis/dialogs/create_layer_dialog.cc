// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/dialogs/create_layer_dialog.h"

#include <memory>
#include <utility>

#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace {

class CreateLayerForm : public View {
 public:
  explicit CreateLayerForm(CreateLayerDialog::Result* sink) : sink_(sink) {
    MarkupRoot loaded = load_markup("dialogs/create_layer.ui.xml");
    if (!loaded.ok()) {
      set_preferred_size({340, 120});
      return;
    }
    name_ = loaded.ids.find_as<Textfield>("name");
    type_ = loaded.ids.find_as<Combobox>("type");

    if (name_) {
      if (sink_ && !sink_->name.empty()) {
        name_->set_text(sink_->name);
      }
      name_->set_change([this]() { flush(); });
    }
    if (type_) {
      type_->set_change([this](int) { flush(); });
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({340, 120});
    add_child(std::move(loaded.root));
    set_preferred_size({340, 120});
    flush();
  }

  ~CreateLayerForm() override { flush(); }

 private:
  void flush() {
    if (!sink_) {
      return;
    }
    if (name_) {
      sink_->name = name_->text();
    }
    if (type_) {
      sink_->geometry_type = type_->selected_text();
    }
  }

  Textfield* name_ = nullptr;
  Combobox* type_ = nullptr;
  CreateLayerDialog::Result* sink_ = nullptr;
};

}  // namespace

bool CreateLayerDialog::run(HWND owner, Result* out) {
  Result local = out ? *out : Result{};
  if (local.geometry_type.empty()) {
    local.geometry_type = "Point";
  }
  auto form = std::make_unique<CreateLayerForm>(&local);
  const Dialog::Result result =
      Dialog::run_modal(owner, L"Create Layer", 400, 200, std::move(form));
  if (result.accepted && out) {
    *out = std::move(local);
    return true;
  }
  return result.accepted;
}

}  // namespace views
}  // namespace ui
