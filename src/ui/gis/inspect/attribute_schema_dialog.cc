// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/inspect/attribute_schema_dialog.h"

#include <memory>
#include <utility>
#include <vector>

#include "ui/gis/form_helpers.h"
#include "ui/gis/scroll_table.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace {

void select_field_type(Combobox* type, const std::string& value) {
  if (!type) {
    return;
  }
  if (value == "Double") {
    type->set_selected_index(1);
  } else if (value == "String") {
    type->set_selected_index(2);
  } else if (value == "Date") {
    type->set_selected_index(3);
  } else {
    type->set_selected_index(0);
  }
}

class AttributeSchemaForm : public View {
 public:
  explicit AttributeSchemaForm(std::vector<AttributeField>* fields)
      : fields_(fields) {
    if (fields_) {
      rows_ = *fields_;
    }
    MarkupRoot loaded = load_markup("inspect/attribute_schema.ui.xml");
    if (!loaded.ok()) {
      set_preferred_size({440, 340});
      return;
    }
    table_ = loaded.ids.find_as<TableView>("fields");
    name_ = loaded.ids.find_as<Textfield>("name");
    type_ = loaded.ids.find_as<Combobox>("type");
    status_ = loaded.ids.find_as<Label>("status");
    add_ = loaded.ids.find_as<Button>("add");
    remove_ = loaded.ids.find_as<Button>("remove");
    if (auto* hint = loaded.ids.find_as<Label>("title_hint")) {
      hint->set_color(Theme::current().text_muted);
    }

    if (name_) {
      name_->set_placeholder("Field name");
      name_->set_change([this]() {
        detail::set_field_invalid(name_, false);
        refresh_status();
      });
      name_->set_submit([this]() { add_field(); });
    }
    if (type_) {
      type_->set_selected_index(2);  // String default
    }
    if (add_) {
      add_->set_style(Button::Style::kPrimary);
      add_->set_click([this]() { add_field(); });
    }
    if (remove_) {
      remove_->set_style(Button::Style::kDestructive);
      remove_->set_click([this]() { remove_field(); });
    }
    if (table_) {
      table_->set_row_click([this](int row) { on_row_selected(row); });
      scroll_ = wrap_markup_table_in_scroll(table_);
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({440, 340});
    add_child(std::move(loaded.root));
    set_preferred_size({440, 340});
    rebuild_table();
    update_remove_enabled();
    refresh_status();
  }

  ~AttributeSchemaForm() override { flush(); }

  bool can_accept() {
    flush();
    detail::set_status_label(status_, true, "");
    return true;
  }

 private:
  void flush() {
    if (fields_) {
      *fields_ = rows_;
    }
  }

  void rebuild_table() {
    if (!table_) {
      return;
    }
    table_->clear_rows();
    for (const AttributeField& field : rows_) {
      table_->add_row({field.name, field.type});
    }
    if (selected_ >= 0 && static_cast<size_t>(selected_) < rows_.size()) {
      table_->set_selected_row(selected_);
    }
    sync_scroll_table_content(table_, scroll_);
  }

  void on_row_selected(int row) {
    selected_ = row;
    update_remove_enabled();
    if (selected_ < 0 || static_cast<size_t>(selected_) >= rows_.size()) {
      return;
    }
    const AttributeField& field = rows_[static_cast<size_t>(selected_)];
    if (name_) {
      name_->set_text(field.name);
      detail::set_field_invalid(name_, false);
    }
    select_field_type(type_, field.type);
    refresh_status();
  }

  void update_remove_enabled() {
    if (!remove_) {
      return;
    }
    const bool has =
        selected_ >= 0 && static_cast<size_t>(selected_) < rows_.size();
    remove_->set_enabled(has);
  }

  bool name_exists(const std::string& name, int except_index) const {
    for (size_t i = 0; i < rows_.size(); ++i) {
      if (static_cast<int>(i) == except_index) {
        continue;
      }
      if (rows_[i].name == name) {
        return true;
      }
    }
    return false;
  }

  void add_field() {
    AttributeField field;
    field.name = name_ ? detail::trim_ascii(name_->text()) : std::string();
    field.type = type_ ? type_->selected_text() : std::string("String");
    if (field.name.empty()) {
      detail::set_field_invalid(name_, true);
      detail::set_status_label(status_, false, "Field name is required.");
      if (name_) {
        name_->request_focus();
      }
      return;
    }
    if (name_exists(field.name, -1)) {
      detail::set_field_invalid(name_, true);
      detail::set_status_label(status_, false,
                              "A field with this name already exists.");
      if (name_) {
        name_->request_focus();
      }
      return;
    }
    rows_.push_back(std::move(field));
    rebuild_table();
    selected_ = static_cast<int>(rows_.size()) - 1;
    if (table_) {
      table_->set_selected_row(selected_);
    }
    if (name_) {
      name_->set_text("");
      detail::set_field_invalid(name_, false);
      name_->request_focus();
    }
    update_remove_enabled();
    flush();
    detail::set_status_label(status_, true, "Field added.");
  }

  void remove_field() {
    if (selected_ < 0 || static_cast<size_t>(selected_) >= rows_.size()) {
      detail::set_status_label(status_, false, "Select a field to remove.");
      return;
    }
    rows_.erase(rows_.begin() + selected_);
    if (selected_ >= static_cast<int>(rows_.size())) {
      selected_ = static_cast<int>(rows_.size()) - 1;
    }
    rebuild_table();
    if (table_ && selected_ >= 0) {
      table_->set_selected_row(selected_);
      on_row_selected(selected_);
    } else if (name_) {
      name_->set_text("");
    }
    update_remove_enabled();
    flush();
    detail::set_status_label(status_, true, "Field removed.");
  }

  void refresh_status() {
    if (!status_) {
      return;
    }
    if (rows_.empty()) {
      detail::set_status_label(status_, true, "No fields yet — add one below.");
    } else if (selected_ >= 0) {
      detail::set_status_label(
          status_, true,
          "Selected row " + std::to_string(selected_ + 1) + " of " +
              std::to_string(rows_.size()) + ".");
    } else {
      detail::set_status_label(
          status_, true,
          std::to_string(rows_.size()) + " field(s). Select a row to edit.");
    }
  }

  std::vector<AttributeField>* fields_ = nullptr;
  std::vector<AttributeField> rows_;
  TableView* table_ = nullptr;
  ScrollView* scroll_ = nullptr;
  Textfield* name_ = nullptr;
  Combobox* type_ = nullptr;
  Button* add_ = nullptr;
  Button* remove_ = nullptr;
  Label* status_ = nullptr;
  int selected_ = -1;
};

}  // namespace

bool AttributeSchemaDialog::run(HWND owner,
                                std::vector<AttributeField>* fields) {
  std::vector<AttributeField> local =
      fields ? *fields : std::vector<AttributeField>();
  auto form = std::make_unique<AttributeSchemaForm>(&local);
  AttributeSchemaForm* form_ptr = form.get();
  const Dialog::Result result = Dialog::run_modal(
      owner, L"Attribute Schema", 500, 440, std::move(form),
      [form_ptr]() { return form_ptr && form_ptr->can_accept(); });
  if (result.accepted && fields) {
    *fields = std::move(local);
    return true;
  }
  return result.accepted;
}

}  // namespace views
}  // namespace ui
