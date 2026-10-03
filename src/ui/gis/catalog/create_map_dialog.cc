// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/catalog/create_map_dialog.h"

#include <memory>
#include <utility>

#include "ui/gis/form_helpers.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace {

class CreateMapForm : public View {
 public:
  explicit CreateMapForm(std::string* sink) : sink_(sink) {
    MarkupRoot loaded = load_markup("catalog/create_map.ui.xml");
    if (!loaded.ok()) {
      set_preferred_size({360, 120});
      return;
    }
    name_ = loaded.ids.find_as<Textfield>("name");
    status_ = loaded.ids.find_as<Label>("status");
    if (auto* hint = loaded.ids.find_as<Label>("title_hint")) {
      hint->set_color(Theme::current().text_muted);
    }

    if (name_) {
      if (sink_ && !sink_->empty()) {
        name_->set_text(*sink_);
      }
      name_->set_placeholder("Untitled map");
      name_->set_change([this]() {
        detail::set_field_invalid(name_, false);
        flush();
        refresh_status();
      });
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({360, 120});
    add_child(std::move(loaded.root));
    set_preferred_size({360, 120});
    flush();
    refresh_status();
  }

  ~CreateMapForm() override { flush(); }

  bool can_accept() {
    flush();
    const bool ok = name_ && !detail::is_blank(name_->text());
    detail::set_field_invalid(name_, !ok);
    if (!ok) {
      detail::set_status_label(status_, false, "Map name is required.");
      if (name_) {
        name_->request_focus();
      }
      return false;
    }
    if (sink_) {
      *sink_ = detail::trim_ascii(name_->text());
    }
    detail::set_status_label(status_, true, "");
    return true;
  }

 private:
  void flush() {
    if (sink_ && name_) {
      *sink_ = name_->text();
    }
  }

  void refresh_status() {
    if (!status_) {
      return;
    }
    if (name_ && detail::is_blank(name_->text())) {
      detail::set_status_label(status_, true, "Enter a map name.");
    } else {
      detail::set_status_label(status_, true, "Ready.");
    }
  }

  Textfield* name_ = nullptr;
  Label* status_ = nullptr;
  std::string* sink_ = nullptr;
};

}  // namespace

bool CreateMapDialog::run(HWND owner, std::string* name) {
  std::string local = name ? *name : std::string();
  auto form = std::make_unique<CreateMapForm>(&local);
  CreateMapForm* form_ptr = form.get();
  const Dialog::Result result = Dialog::run_modal(
      owner, L"Create Map", 420, 220, std::move(form),
      [form_ptr]() { return form_ptr && form_ptr->can_accept(); });
  if (result.accepted && name) {
    *name = std::move(local);
    return true;
  }
  return result.accepted;
}

}  // namespace views
}  // namespace ui
