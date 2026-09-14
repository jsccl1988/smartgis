// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app_commands.h"

#include "ui/views/file_picker.h"
#include "ui/views/message_box.h"

namespace app {

OpenFileCommand run_open_file() {
  OpenFileCommand cmd;
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"GIS vectors\0*.shp;*.gpkg;*.geojson;*.json;*.smt;*.xml\0"
      L"All files\0*.*\0");
  cmd.accepted = picked.accepted;
  cmd.path = picked.path;
  return cmd;
}

void show_open_file_result(const OpenFileCommand& cmd) {
  if (!cmd.accepted) {
    return;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Selected: " + cmd.path);
}

}  // namespace app
