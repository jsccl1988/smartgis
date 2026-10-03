// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/agent_schema.h"

namespace content {
namespace detail {
namespace {

// Keep in sync with dispatch_method / exec_line surfaces.
constexpr const char* kConsolePrefixes[] = {
    ":help",
    ":help json",
    ":clear",
    ":confirm",
    ":log.level ",
    ":refresh",
    ":extent",
    ":layers",
    ":diag",
    ":ask ",
    ":ui find ",
    ":ui click ",
    ":ui type ",
    ":ui tree",
    ":ui overlay",
    ":ui capture",
    ":script ",
    ":record on",
    ":record off",
    ":record poll",
    ":record clear",
    ":gis test",
    ":gis bench",
    ":rhi test",
    ":rhi bench",
    ":sdbd capabilities",
    ":sdbd sql ",
    ":py ",
    ":run ",
};

}  // namespace

std::string agent_help_text() {
  return "commands:\n"
         "  :help | :help json\n"
         "  :clear :confirm :log.level <L>\n"
         "  :refresh :extent :layers :diag [capture]\n"
         "  :ask <text>   (local tool stub; no remote LLM)\n"
         "  :ui find <name>|click <x> <y>|type <text>|tree|overlay|capture [path]\n"
         "  :script <path>  :record on|off|poll|clear\n"
         "  :gis test|bench  :rhi test|bench\n"
         "  :sdbd capabilities|sql <text>\n"
         "  :py <code>  :run <path>\n"
         "rpc: ping log.* cmd.exec py.* ui.* script.run record.* sdbd.* "
         "diag.pack rpc.methods rpc.confirm\n"
         "dangerous ops need :confirm or SG_DEBUG_ALLOW=1 (debug builds auto)";
}

std::string agent_methods_schema_json() {
  // Compact one-line JSON so NDJSON RPC responses stay single-line.
  // Pretty form for editors: tools/debug/agent_methods.json
  return std::string(
      "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\","
      "\"title\":\"SmartGIS DebugAgent methods\","
      "\"transport\":\"loopback NDJSON JSON-RPC\","
      "\"dangerous\":[\"py.eval\",\"py.run_file\",\"sdbd.query\",\"ui.click\","
      "\"ui.capture_shell\"],"
      "\"methods\":{"
      "\"ping\":{\"params\":{},\"result\":{\"pong\":\"boolean\"}},"
      "\"rpc.methods\":{\"params\":{},\"result\":{\"schema\":\"object\"}},"
      "\"rpc.confirm\":{\"params\":{},\"result\":{\"confirmed\":\"boolean\"}},"
      "\"log.tail\":{\"params\":{\"n\":\"integer\"},\"result\":{\"entries\":\"array\"}},"
      "\"log.set_level\":{\"params\":{\"level\":\"string\"},\"result\":{}},"
      "\"log.subscribe\":{\"params\":{},\"result\":{},\"note\":\"pushes event:log\"},"
      "\"cmd.exec\":{\"params\":{\"line\":\"string\"},\"result\":{\"output\":\"string\"}},"
      "\"py.eval\":{\"params\":{\"code\":\"string\",\"confirm\":\"boolean?\"},"
      "\"result\":{\"output\":\"string\"}},"
      "\"py.run_file\":{\"params\":{\"path\":\"string\",\"confirm\":\"boolean?\"},"
      "\"result\":{\"output\":\"string\"}},"
      "\"ui.find\":{\"params\":{\"name\":\"string\"},\"result\":{\"text\":\"string\"}},"
      "\"ui.click\":{\"params\":{\"x\":\"integer\",\"y\":\"integer\","
      "\"button\":\"integer?\",\"confirm\":\"boolean?\"},\"result\":{\"text\":\"string\"}},"
      "\"ui.type\":{\"params\":{\"text\":\"string\"},\"result\":{\"text\":\"string\"}},"
      "\"ui.dump_tree\":{\"params\":{},\"result\":{\"text\":\"string\"}},"
      "\"ui.overlay_stats\":{\"params\":{},\"result\":{\"text\":\"string\"}},"
      "\"ui.capture_shell\":{\"params\":{\"path\":\"string?\",\"confirm\":\"boolean?\"},"
      "\"result\":{\"text\":\"string\"}},"
      "\"script.run\":{\"params\":{\"path\":\"string\"},\"result\":{\"text\":\"string\"}},"
      "\"record.enable\":{\"params\":{\"on\":\"boolean\"},\"result\":{\"on\":\"boolean\"}},"
      "\"record.poll\":{\"params\":{},\"result\":{\"events\":\"array\"}},"
      "\"record.clear\":{\"params\":{},\"result\":{}},"
      "\"sdbd.capabilities\":{\"params\":{},\"result\":{\"ok\":\"boolean\",\"body\":\"string\"}},"
      "\"sdbd.collections\":{\"params\":{},\"result\":{\"ok\":\"boolean\",\"body\":\"string\"}},"
      "\"sdbd.query\":{\"params\":{\"body\":\"string\",\"confirm\":\"boolean?\"},"
      "\"result\":{\"ok\":\"boolean\",\"body\":\"string\"}},"
      "\"diag.pack\":{\"params\":{\"capture\":\"boolean?\",\"log_n\":\"integer?\"},"
      "\"result\":{\"pack\":\"object\"}},"
      "\"shutdown\":{\"params\":{},\"result\":{}}"
      "}}");
}

const char* const* agent_console_command_prefixes(size_t* count) {
  if (count) {
    *count = sizeof(kConsolePrefixes) / sizeof(kConsolePrefixes[0]);
  }
  return kConsolePrefixes;
}

}  // namespace detail
}  // namespace content
