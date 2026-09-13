// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/add_basemap_dialog.h"

#include <memory>
#include <utility>

#include "ui/views/combobox.h"
#include "ui/views/dialog.h"
#include "ui/views/label.h"
#include "ui/views/layout.h"
#include "ui/views/textfield.h"

namespace ui {
namespace views {
namespace {

class AddBasemapForm : public View {
 public:
  explicit AddBasemapForm(AddBasemapDialog::Result* sink) : sink_(sink) {
    auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);

    auto name_label = std::make_unique<Label>("Layer name");
    name_label->set_preferred_size({360, 22});
    auto name = std::make_unique<Textfield>();
    name->set_preferred_size({360, 28});
    if (sink_ && !sink_->name.empty()) {
      name->set_text(sink_->name);
    } else if (name) {
      name->set_text("Basemap");
    }
    name_ = name.get();
    name_->set_change([this]() { flush(); });

    auto kind_label = std::make_unique<Label>("Kind");
    kind_label->set_preferred_size({360, 22});
    auto kind = std::make_unique<Combobox>();
    kind->set_preferred_size({360, 28});
    kind->add_item("xyz");
    kind->add_item("wmts");
    kind->set_selected_index(0);
    kind_ = kind.get();
    kind_->set_change([this](int) { flush(); });

    auto url_label =
        std::make_unique<Label>("URL template ({z}/{x}/{y} or WMTS)");
    url_label->set_preferred_size({360, 22});
    auto url = std::make_unique<Textfield>();
    url->set_preferred_size({360, 28});
    if (sink_ && !sink_->url.empty()) {
      url->set_text(sink_->url);
    }
    url_ = url.get();
    url_->set_change([this]() { flush(); });

    set_layout_manager(std::move(box));
    add_child(std::move(name_label));
    add_child(std::move(name));
    add_child(std::move(kind_label));
    add_child(std::move(kind));
    add_child(std::move(url_label));
    add_child(std::move(url));
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
