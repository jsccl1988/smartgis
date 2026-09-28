// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/dialogs/select_one_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/input/combobox.h"

namespace ui {
namespace views {
namespace {

class SelectOneForm : public View {
 public:
  SelectOneForm(const std::vector<std::string>& items, int* index_sink)
      : index_sink_(index_sink) {
    MarkupRoot loaded = load_markup("toolkit/select_one.ui.xml");
    if (!loaded.ok()) {
      set_preferred_size({340, 80});
      return;
    }
    combo_ = loaded.ids.find_as<Combobox>("choice");
    if (combo_) {
      for (const std::string& item : items) {
        combo_->add_item(item);
      }
      if (!items.empty()) {
        combo_->set_selected_index(0);
        if (index_sink_) {
          *index_sink_ = 0;
        }
      }
      combo_->set_change([this](int i) {
        if (index_sink_) {
          *index_sink_ = i;
        }
      });
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({340, 80});
    add_child(std::move(loaded.root));
    set_preferred_size({340, 80});
  }

  ~SelectOneForm() override {
    if (index_sink_ && combo_) {
      *index_sink_ = combo_->selected_index();
    }
  }

 private:
  Combobox* combo_ = nullptr;
  int* index_sink_ = nullptr;
};

}  // namespace

bool SelectOneDialog::run(HWND owner, const std::vector<std::string>& items,
                          std::string* out) {
  if (items.empty()) {
    return false;
  }
  int index = 0;
  auto form = std::make_unique<SelectOneForm>(items, &index);
  const Dialog::Result result =
      Dialog::run_modal(owner, L"Select One", 380, 180, std::move(form));
  if (!result.accepted) {
    return false;
  }
  if (index < 0 || static_cast<size_t>(index) >= items.size()) {
    return false;
  }
  if (out) {
    *out = items[static_cast<size_t>(index)];
  }
  return true;
}

bool SelectOneDialog::run(HWND owner, const std::vector<unsigned int>& ids,
                          unsigned int* out) {
  if (ids.empty()) {
    return false;
  }
  std::vector<std::string> labels;
  labels.reserve(ids.size());
  for (unsigned int id : ids) {
    labels.push_back(std::to_string(id));
  }
  std::string selected;
  if (!run(owner, labels, &selected)) {
    return false;
  }
  if (!out) {
    return true;
  }
  for (unsigned int id : ids) {
    if (std::to_string(id) == selected) {
      *out = id;
      return true;
    }
  }
  return false;
}

}  // namespace views
}  // namespace ui
