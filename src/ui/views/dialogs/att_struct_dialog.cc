// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/dialogs/att_struct_dialog.h"

#include <memory>
#include <utility>

#include "ui/views/primitives/button.h"
#include "ui/views/primitives/combobox.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/primitives/label.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/primitives/table_view.h"
#include "ui/views/primitives/textfield.h"

namespace ui {
namespace views {
namespace {

const char* kTypeLabels[] = {"Integer", "Double", "String", "Date"};

class AttStructForm : public View {
 public:
  explicit AttStructForm(std::vector<AttField>* fields) : fields_(fields) {
    if (fields_) {
      rows_ = *fields_;
    }
    auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);

    auto table = std::make_unique<TableView>();
    table->set_columns({"Name", "Type"});
    table->set_preferred_size({400, 160});
    table_ = table.get();
    table_->set_row_click([this](int row) { selected_ = row; });

    auto name_label = std::make_unique<Label>("Field name");
    name_label->set_preferred_size({400, 22});
    auto name = std::make_unique<Textfield>();
    name->set_preferred_size({400, 28});
    name_ = name.get();

    auto type_label = std::make_unique<Label>("Field type");
    type_label->set_preferred_size({400, 22});
    auto type = std::make_unique<Combobox>();
    type->set_preferred_size({400, 28});
    for (const char* label : kTypeLabels) {
      type->add_item(label);
    }
    type->set_selected_index(0);
    type_ = type.get();

    auto buttons = std::make_unique<View>();
    auto row_box =
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
    auto add = std::make_unique<Button>("Add");
    add->set_preferred_size({80, 28});
    add->set_click([this]() { add_field(); });
    auto remove = std::make_unique<Button>("Remove");
    remove->set_preferred_size({80, 28});
    remove->set_click([this]() { remove_field(); });
    buttons->set_layout_manager(std::move(row_box));
    buttons->add_child(std::move(add));
    buttons->add_child(std::move(remove));
    buttons->set_preferred_size({400, 32});

    box->set_flex_for_view(table_, 1);
    set_layout_manager(std::move(box));
    add_child(std::move(table));
    add_child(std::move(name_label));
    add_child(std::move(name));
    add_child(std::move(type_label));
    add_child(std::move(type));
    add_child(std::move(buttons));
    set_preferred_size({420, 280});
    rebuild_table();
  }

  ~AttStructForm() override { flush(); }

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
    for (const AttField& field : rows_) {
      table_->add_row({field.name, field.type});
    }
  }

  void add_field() {
    AttField field;
    field.name = name_ ? name_->text() : std::string();
    field.type = type_ ? type_->selected_text() : std::string("String");
    if (field.name.empty()) {
      return;
    }
    rows_.push_back(std::move(field));
    rebuild_table();
    selected_ = static_cast<int>(rows_.size()) - 1;
    if (table_) {
      table_->set_selected_row(selected_);
    }
    flush();
  }

  void remove_field() {
    if (selected_ < 0 || static_cast<size_t>(selected_) >= rows_.size()) {
      return;
    }
    rows_.erase(rows_.begin() + selected_);
    if (selected_ >= static_cast<int>(rows_.size())) {
      selected_ = static_cast<int>(rows_.size()) - 1;
    }
    rebuild_table();
    if (table_ && selected_ >= 0) {
      table_->set_selected_row(selected_);
    }
    flush();
  }

  std::vector<AttField>* fields_ = nullptr;
  std::vector<AttField> rows_;
  TableView* table_ = nullptr;
  Textfield* name_ = nullptr;
  Combobox* type_ = nullptr;
  int selected_ = -1;
};

}  // namespace

bool AttStructDialog::run(HWND owner, std::vector<AttField>* fields) {
  std::vector<AttField> local = fields ? *fields : std::vector<AttField>();
  auto form = std::make_unique<AttStructForm>(&local);
  const Dialog::Result result =
      Dialog::run_modal(owner, L"Attribute Structure", 460, 360,
                        std::move(form));
  if (result.accepted && fields) {
    *fields = std::move(local);
    return true;
  }
  return result.accepted;
}

}  // namespace views
}  // namespace ui
