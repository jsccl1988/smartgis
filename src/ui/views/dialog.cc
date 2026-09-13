// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/dialog.h"

#include "ui/views/button.h"
#include "ui/views/layout.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {
namespace {

thread_local Dialog* t_current = nullptr;

// Wraps caller contents with OK / Cancel so forms do not each rebuild chrome.
class DialogChrome : public View {
 public:
  explicit DialogChrome(std::unique_ptr<View> body) {
    auto root = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
    auto buttons = std::make_unique<View>();
    buttons->set_preferred_size({0, 40});
    auto row = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
    auto ok = std::make_unique<Button>("OK");
    ok->set_preferred_size({88, 28});
    ok->set_click([] { Dialog::close(true); });
    auto cancel = std::make_unique<Button>("Cancel");
    cancel->set_preferred_size({88, 28});
    cancel->set_click([] { Dialog::close(false); });
    View* body_ptr = body.get();
    buttons->set_layout_manager(std::move(row));
    buttons->add_child(std::move(ok));
    buttons->add_child(std::move(cancel));
    root->set_flex_for_view(body_ptr, 1);
    set_layout_manager(std::move(root));
    add_child(std::move(body));
    add_child(std::move(buttons));
  }
};

}  // namespace

Dialog::Result Dialog::run_modal(HWND owner, const wchar_t* title, int w, int h,
                                 std::unique_ptr<View> contents) {
  Result result;
  Widget widget;
  Widget::InitParams params;
  params.title = title ? title : L"SmartGIS";
  params.width = w;
  params.height = h;
  params.owner = owner;
  if (!widget.init(params)) {
    return result;
  }
  auto chrome = std::make_unique<DialogChrome>(std::move(contents));
  widget.set_contents_view(std::move(chrome));

  Dialog dialog;
  dialog.widget_ = &widget;
  Dialog* previous = t_current;
  t_current = &dialog;

  const bool owner_was_enabled = owner && IsWindowEnabled(owner);
  if (owner) {
    EnableWindow(owner, FALSE);
  }
  widget.run_modal();
  if (owner) {
    EnableWindow(owner, owner_was_enabled ? TRUE : FALSE);
    if (IsWindow(owner)) {
      SetForegroundWindow(owner);
    }
  }

  t_current = previous;
  result.accepted = dialog.accepted_;
  return result;
}

void Dialog::close(bool accepted) {
  if (!t_current) {
    return;
  }
  t_current->accepted_ = accepted;
  if (t_current->widget_) {
    t_current->widget_->request_close();
  }
}

}  // namespace views
}  // namespace ui
