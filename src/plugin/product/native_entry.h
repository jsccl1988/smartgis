// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_NATIVE_ENTRY_H_
#define PLUGIN_PRODUCT_NATIVE_ENTRY_H_

// Include from a pack's dll_entry.cc after defining:
//   PLUGIN_PACK_ID          "smartgis.map2d"
//   PLUGIN_PACK_REGISTER(h) plugin::register_map2d(h)

#include "content/public/plugin_host.h"
#include "tool/command/command.h"

#ifndef PLUGIN_PACK_ID
#error "Define PLUGIN_PACK_ID before including native_entry.h"
#endif
#ifndef PLUGIN_PACK_REGISTER
#error "Define PLUGIN_PACK_REGISTER(host) before including native_entry.h"
#endif

namespace {

content::PluginHost* g_pack_host = nullptr;

}  // namespace

extern "C" {

__declspec(dllexport) int init(void* host) {
  g_pack_host = static_cast<content::PluginHost*>(host);
  if (!g_pack_host || !PLUGIN_PACK_REGISTER(g_pack_host)) {
    g_pack_host = nullptr;
    return 1;
  }
  return 0;
}

__declspec(dllexport) int run(const char* event_id, const char* payload) {
  if (!g_pack_host || !event_id || !*event_id) {
    return 0;
  }
  tool::CommandArgs args;
  args.payload = payload ? payload : "";
  return g_pack_host->execute(event_id, args) ? 1 : 0;
}

__declspec(dllexport) void destroy() {
  if (g_pack_host) {
    g_pack_host->withdraw(PLUGIN_PACK_ID);
    g_pack_host = nullptr;
  }
}

}  // extern "C"

#endif  // PLUGIN_PRODUCT_NATIVE_ENTRY_H_
