// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/dialogs/dialog.h"

#include "ui/views/primitives/button.h"
#include "ui/views/kernel/dpi.h"
#include "ui/views/kernel/event.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/kernel/widget.h"

namespace ui {
namespace views {
namespace {

thread_local Dialog* t_current = nullptr;

// Wraps caller contents with OK / Cancel so forms do not each rebuild chrome.
class DialogChrome : public View {
 public:
  explicit DialogChrome(std::unique_ptr<View> body) {
    const float scale = 1.f;
    auto root = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
    root->set_inside_border(dip_to_px(8, scale));
    root->set_between_child_spacing(dip_to_px(8, scale));

    auto buttons = std::make_unique<View>();
    buttons->set_preferred_size({0, dip_to_px(40, scale)});
    auto row = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
    row->set_between_child_spacing(dip_to_px(8, scale));

    // Flex spacer pushes OK/Cancel to the trailing edge (common dialog chrome).
    auto spacer = std::make_unique<View>();
    View* spacer_ptr = spacer.get();
    auto ok = std::make_unique<Button>("OK");
    ok->set_preferred_size({dip_to_px(88, scale), dip_to_px(28, scale)});
    ok->set_click([] { Dialog::close(true); });
    auto cancel = std::make_unique<Button>("Cancel");
    cancel->set_preferred_size({dip_to_px(88, scale), dip_to_px(28, scale)});
    cancel->set_click([] { Dialog::close(false); });

    View* body_ptr = body.get();
    row->set_flex_for_view(spacer_ptr, 1);
    buttons->set_layout_manager(std::move(row));
    buttons->add_child(std::move(spacer));
    buttons->add_child(std::move(ok));
    buttons->add_child(std::move(cancel));

    root->set_flex_for_view(body_ptr, 1);
    set_layout_manager(std::move(root));
    add_child(std::move(body));
    add_child(std::move(buttons));
    set_focusable(true);
  }

  bool on_key_event(const KeyEvent& event) override {
    if (event.type == KeyEvent::Type::kDown) {
      if (event.vk == VK_ESCAPE) {
        Dialog::close(false);
        return true;
      }
      if (event.vk == VK_RETURN) {
        Dialog::close(true);
        return true;
      }
    }
    return View::on_key_event(event);
  }
};

}  // namespace

Dialog::Result Dialog::run_modal(HWND owner, const wchar_t* title, int w, int h,
                                 std::unique_ptr<View> contents) {
  Result result;
  Widget widget;
  Widget::InitParams params;
  params.title = title ? title : L"SmartGIS";
  // Client DIPs; Widget + dialog_host expand / center / clamp for the owner.
  params.width = w;
  params.height = h;
  params.size_in_dips = true;
  params.owner = owner;
  if (!widget.init(params)) {
    return result;
  }
  auto chrome = std::make_unique<DialogChrome>(std::move(contents));
  DialogChrome* chrome_ptr = chrome.get();
  widget.set_contents_view(std::move(chrome));
  chrome_ptr->request_focus();

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
