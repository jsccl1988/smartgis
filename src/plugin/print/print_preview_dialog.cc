// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/print/print_preview_dialog.h"

#include <memory>

#include "plugin/widgets/map_preview.h"
#include "ui/views/button.h"
#include "ui/views/file_picker.h"

namespace plugin {
namespace {

constexpr wchar_t kImageFilter[] =
    L"Image Files (*.bmp;*.gif;*.jpg;*.png;*.tif)\0*.bmp;*.gif;*.jpg;*.png;*.tif\0"
    L"All Files (*.*)\0*.*\0";

}  // namespace

PrintPreviewDialog::PrintPreviewDialog() {
  add_child(std::make_unique<MapPreviewView>());

  auto save = std::make_unique<ui::views::Button>("Save");
  save->set_click([] {
    (void)ui::views::pick_save_file(kImageFilter);
  });
  add_child(std::move(save));

  set_preferred_size({520, 400});
}

}  // namespace plugin
