// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/dialogs/add_basemap_dialog.h"

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

class AddBasemapForm : public View {
 public:
  explicit AddBasemapForm(AddBasemapDialog::Result* sink) : sink_(sink) {
    MarkupRoot loaded = load_markup("dialogs/add_basemap.ui.xml");
    if (!loaded.ok()) {
      // Fallback empty host so Dialog still opens if resources are missing.
      set_preferred_size({380, 170});
      return;
    }
    name_ = loaded.ids.find_as<Textfield>("name");
    kind_ = loaded.ids.find_as<Combobox>("kind");
    url_ = loaded.ids.find_as<Textfield>("url");

    if (name_) {
      if (sink_ && !sink_->name.empty()) {
        name_->set_text(sink_->name);
      } else {
        name_->set_text("Basemap");
      }
      name_->set_change([this]() { flush(); });
    }
    if (kind_) {
      kind_->set_change([this](int) { flush(); });
    }
    if (url_) {
      if (sink_ && !sink_->url.empty()) {
        url_->set_text(sink_->url);
      }
      url_->set_change([this]() { flush(); });
    }

    // Adopt markup root as sole child; fill host.
    auto fill = std::make_unique<FillLayout>();
    set_layout_manager(std::move(fill));
    loaded.root->set_preferred_size({380, 170});
    add_child(std::move(loaded.root));
    set_preferred_size({380, 170});
    flush();
  }

  ~AddBasemapForm() override { flush(); }

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

  Textfield* name_ = nullptr;
  Combobox* kind_ = nullptr;
  Textfield* url_ = nullptr;
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
  const Dialog::Result result =
      Dialog::run_modal(owner, L"Add online basemap", 440, 260, std::move(form));
  if (result.accepted && out) {
    *out = std::move(local);
    return true;
  }
  return result.accepted;
}

}  // namespace views
}  // namespace ui
