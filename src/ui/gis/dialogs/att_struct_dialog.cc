// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/dialogs/att_struct_dialog.h"

#include <memory>
#include <utility>

#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace {

class AttStructForm : public View {
 public:
  explicit AttStructForm(std::vector<AttField>* fields) : fields_(fields) {
    if (fields_) {
      rows_ = *fields_;
    }
    MarkupRoot loaded = load_markup("dialogs/att_struct.ui.xml");
    if (!loaded.ok()) {
      set_preferred_size({420, 280});
      return;
    }
    table_ = loaded.ids.find_as<TableView>("fields");
    name_ = loaded.ids.find_as<Textfield>("name");
    type_ = loaded.ids.find_as<Combobox>("type");
    auto* add = loaded.ids.find_as<Button>("add");
    auto* remove = loaded.ids.find_as<Button>("remove");

    if (table_) {
      table_->set_row_click([this](int row) { selected_ = row; });
    }
    if (add) {
      add->set_click([this]() { add_field(); });
    }
    if (remove) {
      remove->set_click([this]() { remove_field(); });
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({420, 280});
    add_child(std::move(loaded.root));
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
