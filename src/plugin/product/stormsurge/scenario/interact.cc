// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/scenario/interact.h"

#include <string_view>

#include "content/browser/capability/host.h"
#include "plugin/runtime/host/capability/scenario.h"

namespace plugin {
namespace {

bool exec_stormsurge_run(content::CapabilityHost& host, std::string_view) {
  return host.run_plugin_command &&
         host.run_plugin_command("stormsurge.scenario.showcase");
}

}  // namespace

void register_stormsurge_interact_verbs() {
  register_showcase_verb("stormsurge_run", exec_stormsurge_run, "");
}

}  // namespace plugin
