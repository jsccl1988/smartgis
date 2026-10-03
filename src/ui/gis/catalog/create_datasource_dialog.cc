// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/catalog/create_datasource_dialog.h"

#include <memory>
#include <utility>

#include "ui/gis/form_helpers.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace {

void select_type(Combobox* type, const std::string& value) {
  if (!type) {
    return;
  }
  // Items are MEM / file / GPKG in markup order.
  if (value == "file") {
    type->set_selected_index(1);
  } else if (value == "GPKG" || value == "gpkg") {
    type->set_selected_index(2);
  } else {
    type->set_selected_index(0);
  }
}

class CreateDatasourceForm : public View {
 public:
  explicit CreateDatasourceForm(CreateDatasourceDialog::Result* sink)
      : sink_(sink) {
    MarkupRoot loaded = load_markup("catalog/create_datasource.ui.xml");
    if (!loaded.ok()) {
      set_preferred_size({360, 200});
      return;
    }
    name_ = loaded.ids.find_as<Textfield>("name");
    type_ = loaded.ids.find_as<Combobox>("type");
    status_ = loaded.ids.find_as<Label>("status");
    if (auto* hint = loaded.ids.find_as<Label>("title_hint")) {
      hint->set_color(Theme::current().text_muted);
    }
    if (auto* type_hint = loaded.ids.find_as<Label>("type_hint")) {
      type_hint->set_color(Theme::current().text_muted);
    }

    if (name_) {
      if (sink_ && !sink_->name.empty()) {
        name_->set_text(sink_->name);
      }
      name_->set_placeholder("e.g. cities");
      name_->set_change([this]() {
        detail::set_field_invalid(name_, false);
        flush();
        refresh_status();
      });
    }
    if (type_) {
      select_type(type_, sink_ ? sink_->type : std::string("MEM"));
      type_->set_change([this](int) { flush(); });
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({360, 200});
    add_child(std::move(loaded.root));
    set_preferred_size({360, 200});
    flush();
    refresh_status();
  }

  ~CreateDatasourceForm() override { flush(); }

  bool can_accept() {
    flush();
    const bool ok = name_ && !detail::is_blank(name_->text());
    detail::set_field_invalid(name_, !ok);
    if (!ok) {
      detail::set_status_label(status_, false, "Datasource name is required.");
      if (name_) {
        name_->request_focus();
      }
      return false;
    }
    if (sink_) {
      sink_->name = detail::trim_ascii(name_->text());
    }
    detail::set_status_label(status_, true, "");
    return true;
  }

 private:
  void flush() {
    if (!sink_) {
      return;
    }
    if (name_) {
      sink_->name = name_->text();
    }
    if (type_) {
      sink_->type = type_->selected_text();
    }
  }

  void refresh_status() {
    if (!status_) {
      return;
    }
    if (name_ && detail::is_blank(name_->text())) {
      detail::set_status_label(status_, true, "Enter a datasource name.");
    } else {
      detail::set_status_label(status_, true, "Ready.");
    }
  }

  Textfield* name_ = nullptr;
  Combobox* type_ = nullptr;
  Label* status_ = nullptr;
  CreateDatasourceDialog::Result* sink_ = nullptr;
};

}  // namespace

bool CreateDatasourceDialog::run(HWND owner, Result* out) {
  Result local = out ? *out : Result{};
  if (local.type.empty()) {
    local.type = "MEM";
  }
  auto form = std::make_unique<CreateDatasourceForm>(&local);
  CreateDatasourceForm* form_ptr = form.get();
  const Dialog::Result result = Dialog::run_modal(
      owner, L"Create Datasource", 420, 280, std::move(form),
      [form_ptr]() { return form_ptr && form_ptr->can_accept(); });
  if (result.accepted && out) {
    *out = std::move(local);
    return true;
  }
  return result.accepted;
}

}  // namespace views
}  // namespace ui
