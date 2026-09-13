// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/print/print_commands.h"

#include "content/public/plugin_host.h"
#include "plugin/print/print_preview_dialog.h"
#include "tool/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.print";
constexpr const char* kPreviewId = "print.preview";

}  // namespace

bool register_print(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  content::DialogContribution dialog{kPreviewId, "Print preview"};
  if (!host->contribute_dialog(
          kPluginId, dialog, [](content::PluginHost* h) {
            PrintPreviewDialog preview;
            (void)h;
          })) {
    return false;
  }

  return host->contribute_command(
      kPluginId, kPreviewId, "Print preview", "file",
      [host](const tool::CommandArgs&) {
        return host->open_dialog(kPreviewId);
      });
}

}  // namespace plugin
