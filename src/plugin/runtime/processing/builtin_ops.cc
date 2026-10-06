// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/processing/builtin_ops.h"

#include <string>

#include "app/views/browser/plugin/builtins.h"
#include "content/public/plugin_host.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.processing";

}  // namespace

bool register_builtin_processing(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  for (const BuiltinOpDesc& op : builtin_op_catalog()) {
    content::ProcessingContribution proc;
    proc.id = op.id;
    proc.title = op.title;
    const std::string id = op.id;
    if (!host->contribute_processing(
            kPluginId, proc,
            [id](content::PluginHost*, std::string_view args) {
              return run_builtin_op(id, args);
            })) {
      return false;
    }
  }
  return true;
}

}  // namespace plugin

namespace {
[[maybe_unused]] const bool k_appended = app::append_builtin_plugin({
    .id = "smartgis.processing",
    .name = "Processing",
    .start = plugin::register_builtin_processing,
    .resource_package = nullptr,
});
}  // namespace
