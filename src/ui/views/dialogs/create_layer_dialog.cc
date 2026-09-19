// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/dialogs/create_layer_dialog.h"

#include <memory>
#include <utility>

#include "ui/views/primitives/combobox.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/primitives/label.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/primitives/textfield.h"

namespace ui {
namespace views {
namespace {

class CreateLayerForm : public View {
 public:
  explicit CreateLayerForm(CreateLayerDialog::Result* sink) : sink_(sink) {
    auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
    auto name_label = std::make_unique<Label>("Layer name");
    name_label->set_preferred_size({320, 22});
    auto name = std::make_unique<Textfield>();
    name->set_preferred_size({320, 28});
    if (sink_ && !sink_->name.empty()) {
      name->set_text(sink_->name);
    }
    name_ = name.get();
    name_->set_change([this]() { flush(); });

    auto type_label = std::make_unique<Label>("Geometry type");
    type_label->set_preferred_size({320, 22});
    auto type = std::make_unique<Combobox>();
    type->set_preferred_size({320, 28});
    type->add_item("Point");
    type->add_item("Line");
    type->add_item("Polygon");
    type->set_selected_index(0);
    type_ = type.get();
    type_->set_change([this](int) { flush(); });

    set_layout_manager(std::move(box));
    add_child(std::move(name_label));
    add_child(std::move(name));
    add_child(std::move(type_label));
    add_child(std::move(type));
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
