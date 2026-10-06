// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/print/views/print_preview_dialog.h"

#include <memory>
#include <string>

#include "content/public/plugin_host.h"
#include "plugin/product/map2d/print/composer/print_composer.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "plugin/runtime/widgets/present_surface_picker.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"

namespace plugin {
namespace {

constexpr wchar_t kImageFilter[] =
    L"Image Files (*.bmp;*.gif;*.jpg;*.png;*.tif)\0*.bmp;*.gif;*.jpg;*.png;*.tif\0"
    L"All Files (*.*)\0*.*\0";

bool export_composed_page(const std::string& path) {
  PrintComposerInput in;
  in.page_width_px = 800;
  in.page_height_px = 600;
  in.map_units_per_px = 50.0;
  in.scale_label = "1:50000";
  in.legend = {{"Basemap", 0xff88aa66},
               {"Roads", 0xffccaa44},
               {"Labels", 0xff333333}};
  return PrintComposer::export_page_bmp(in, path);
}

}  // namespace

PrintPreviewDialog::PrintPreviewDialog(content::PluginHost* host) {
  (void)host;
  auto save = std::make_unique<ui::views::Button>("Save");
  save->set_click([this] {
    const ui::views::FilePickerResult picked =
        ui::views::pick_save_file(kImageFilter);
    if (!picked.accepted || picked.path.empty()) {
      return;
    }
    if (!export_composed_page(picked.path)) {
      set_operation_result(
          "{\"error\":\"export_not_implemented\",\"op\":\"print.save\"}");
      ui::views::show_message_box(
          ui::views::MessageBoxKind::kError,
          "Export is not implemented.");
      return;
    }
    set_operation_result("{\"ok\":true,\"op\":\"print.save\",\"layout\":true}");
  });
  add_child(std::move(save));

  set_preferred_size({360, 120});
}

void open_print_preview_dialog(content::PluginHost* host) {
  if (!host) {
    return;
  }
  auto body = wrap_with_present_surface(
      host, std::make_unique<PrintPreviewDialog>(host));
  const int width = body->preferred_size().width;
  const int height = body->preferred_size().height;
  show_owned_dialog(L"地图打印", width, height, std::move(body));
}

}  // namespace plugin
