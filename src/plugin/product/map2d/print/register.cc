// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/print/register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/map2d/print/views/print_preview_dialog.h"
#include "tool/command/command.h"

namespace plugin {
namespace detail {
namespace {

constexpr const char* kPluginId = "smartgis.map2d";
constexpr const char* kPreviewId = "print.preview";

}  // namespace

bool contribute_print(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  // Catalog only: store the factory. Do not construct Views, open a Widget,
  // or call present_dataset here (china seed / Widget.init must stay quiet).
  content::DialogContribution dialog{kPreviewId, "打印"};
  if (!host->contribute_dialog(
          kPluginId, dialog, [](content::PluginHost* h) {
            open_print_preview_dialog(h);
          })) {
    return false;
  }

  return host->contribute_command(
      kPluginId, kPreviewId, "打印", "file",
      [host](const tool::CommandArgs&) {
        if (!host) {
          return false;
        }
        return host->open_dialog(kPreviewId);
      });
}

}  // namespace detail
}  // namespace plugin
