// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/print/print_preview_dialog.h"

#include <memory>

#include "plugin/runtime/host/operation_result.h"
#include "plugin/runtime/widgets/map_preview.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/dialogs/shell/file_picker.h"
#include "ui/views/dialogs/shell/message_box.h"

namespace plugin {
namespace {

constexpr wchar_t kImageFilter[] =
    L"Image Files (*.bmp;*.gif;*.jpg;*.png;*.tif)\0*.bmp;*.gif;*.jpg;*.png;*.tif\0"
    L"All Files (*.*)\0*.*\0";

}  // namespace

PrintPreviewDialog::PrintPreviewDialog() {
  auto preview = std::make_unique<MapPreviewView>();
  preview_ = preview.get();
  add_child(std::move(preview));

  auto save = std::make_unique<ui::views::Button>("Save");
  save->set_click([this] {
    const ui::views::FilePickerResult picked =
        ui::views::pick_save_file(kImageFilter);
    if (!picked.accepted || picked.path.empty()) {
      return;
    }
    if (!preview_ || !preview_->export_bmp(picked.path)) {
      set_operation_result(
          "{\"error\":\"export_not_implemented\",\"op\":\"print.save\"}");
      ui::views::show_message_box(
          ui::views::MessageBoxKind::kError,
          "Export is not implemented.");
      return;
    }
    set_operation_result("{\"ok\":true,\"op\":\"print.save\"}");
  });
  add_child(std::move(save));

  set_preferred_size({520, 400});
}

}  // namespace plugin
