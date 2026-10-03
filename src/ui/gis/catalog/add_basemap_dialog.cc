// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/catalog/add_basemap_dialog.h"

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

class AddBasemapForm : public View {
 public:
  explicit AddBasemapForm(AddBasemapDialog::Result* sink) : sink_(sink) {
    MarkupRoot loaded = load_markup("catalog/add_basemap.ui.xml");
    if (!loaded.ok()) {
      set_preferred_size({420, 260});
      return;
    }
    name_ = loaded.ids.find_as<Textfield>("name");
    kind_ = loaded.ids.find_as<Combobox>("kind");
    url_ = loaded.ids.find_as<Textfield>("url");
    status_ = loaded.ids.find_as<Label>("status");
    auto* hint = loaded.ids.find_as<Label>("title_hint");
    auto* url_hint = loaded.ids.find_as<Label>("url_hint");
    if (hint) {
      hint->set_color(Theme::current().text_muted);
    }
    if (url_hint) {
      url_hint->set_color(Theme::current().text_muted);
    }

    if (name_) {
      if (sink_ && !sink_->name.empty()) {
        name_->set_text(sink_->name);
      } else {
        name_->set_text("Basemap");
      }
      name_->set_placeholder("Display name");
      name_->set_change([this]() {
        detail::set_field_invalid(name_, false);
        flush();
        refresh_status();
      });
    }
    if (kind_) {
      if (sink_ && sink_->kind == "wmts") {
        kind_->set_selected_index(1);
      } else {
        kind_->set_selected_index(0);
      }
      kind_->set_change([this](int) { flush(); });
    }
    if (url_) {
      if (sink_ && !sink_->url.empty()) {
        url_->set_text(sink_->url);
      }
      url_->set_placeholder("https://…/{z}/{x}/{y}.png");
      url_->set_change([this]() {
        detail::set_field_invalid(url_, false);
        flush();
        refresh_status();
      });
    }

    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({420, 260});
    add_child(std::move(loaded.root));
    set_preferred_size({420, 260});
    flush();
    refresh_status();
  }

  ~AddBasemapForm() override { flush(); }

  bool can_accept() {
    flush();
    const bool name_ok = name_ && !detail::is_blank(name_->text());
    const bool url_ok = url_ && !detail::is_blank(url_->text());
    detail::set_field_invalid(name_, !name_ok);
    detail::set_field_invalid(url_, !url_ok);
    if (!name_ok) {
      detail::set_status_label(status_, false, "Layer name is required.");
      if (name_) {
        name_->request_focus();
      }
      return false;
    }
    if (!url_ok) {
      detail::set_status_label(status_, false, "URL template is required.");
      if (url_) {
        url_->request_focus();
      }
      return false;
    }
    // Persist trimmed values into the sink.
    if (sink_) {
      sink_->name = detail::trim_ascii(name_->text());
      sink_->url = detail::trim_ascii(url_->text());
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
    if (kind_) {
      sink_->kind = kind_->selected_text();
    }
    if (url_) {
      sink_->url = url_->text();
    }
  }

  void refresh_status() {
    if (!status_) {
      return;
    }
    if (name_ && detail::is_blank(name_->text())) {
      detail::set_status_label(status_, true, "Enter a layer name.");
      return;
    }
    if (url_ && detail::is_blank(url_->text())) {
      detail::set_status_label(status_, true, "Paste an XYZ or WMTS URL.");
      return;
    }
    detail::set_status_label(status_, true, "Ready.");
  }

  Textfield* name_ = nullptr;
  Combobox* kind_ = nullptr;
  Textfield* url_ = nullptr;
  Label* status_ = nullptr;
  AddBasemapDialog::Result* sink_ = nullptr;
};

}  // namespace

bool AddBasemapDialog::run(HWND owner, Result* out) {
  Result local = out ? *out : Result{};
  if (local.kind.empty()) {
    local.kind = "xyz";
  }
  if (local.name.empty()) {
    local.name = "Basemap";
  }
  auto form = std::make_unique<AddBasemapForm>(&local);
  AddBasemapForm* form_ptr = form.get();
  const Dialog::Result result = Dialog::run_modal(
      owner, L"Add online basemap", 480, 360, std::move(form),
      [form_ptr]() { return form_ptr && form_ptr->can_accept(); });
  if (result.accepted && out) {
    *out = std::move(local);
    return true;
  }
  return result.accepted;
}

}  // namespace views
}  // namespace ui
