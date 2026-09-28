// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/print/print_commands.h"

#include <memory>

#include "content/public/plugin_host.h"
#include "plugin/product/print/print_preview_dialog.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.print";
constexpr const char* kPreviewId = "print.preview";

}  // namespace

bool register_print(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  // Views path: PrintPreviewDialog (ui::views::View). Do not open leftover CDlg.
  content::DialogContribution dialog{kPreviewId, "Print preview"};
  if (!host->contribute_dialog(
          kPluginId, dialog, [](content::PluginHost* h) {
            if (!h) {
              return;
            }
            auto body = std::make_unique<PrintPreviewDialog>();
            const int width = body->preferred_size().width;
            const int height = body->preferred_size().height;
            show_owned_dialog(L"Print preview", width, height, std::move(body));
          })) {
    return false;
  }

  return host->contribute_command(
      kPluginId, kPreviewId, "Print preview", "file",
      [host](const tool::CommandArgs&) {
        if (!host) {
          return false;
        }
        return host->open_dialog(kPreviewId);
      });
}

}  // namespace plugin
