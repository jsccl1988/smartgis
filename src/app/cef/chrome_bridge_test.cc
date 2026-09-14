// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/chrome_bridge.h"

#include <cstdio>
#include <cstdlib>

namespace {

int g_fails = 0;

void expect(bool cond, const char* label) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", label);
    ++g_fails;
  } else {
    std::printf("ok: %s\n", label);
  }
}

void test_parse_activate_tool() {
  const char* json =
      R"({"api_version":1,"type":"ActivateTool","request_id":"1",)"
      R"("view_id":1,"command_id":"selection.point"})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(json, &msg), "parse ok");
  expect(msg.api_version == 1, "ver");
  expect(msg.type == app::cef::BridgeType::kActivateTool, "type");
  expect(msg.command_id == "selection.point", "cmd");
}

void test_reject_bad_version() {
  const char* json =
      R"({"api_version":99,"type":"Exit","request_id":"x","view_id":0})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(json, &msg), "parse structure");
  expect(!app::cef::detail::is_supported_api_version(msg.api_version),
         "reject");
}

void test_parse_topic_tool_command() {
  const char* json =
      R"({"topic":"tool.command","payload":{"command":"view.pan"}})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(json, &msg), "topic parse");
  expect(msg.type == app::cef::BridgeType::kActivateTool, "topic type");
  expect(msg.command_id == "view.pan", "topic cmd");
  expect(app::cef::detail::is_supported_api_version(msg.api_version),
         "topic ver");
}

void test_parse_topic_panel_action() {
  const char* json =
      R"({"topic":"panel.action","payload":{"command":"selection.clear"}})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(json, &msg), "panel parse");
  expect(msg.type == app::cef::BridgeType::kActivateTool, "panel type");
  expect(msg.command_id == "selection.clear", "panel cmd");
}

void test_parse_topic_workspace_ids() {
  const char* json =
      R"({"topic":"tool.command","payload":{"command":"view3d.trackball",)"
      R"("view_id":2}})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(json, &msg), "ws id parse");
  expect(msg.command_id == "view3d.trackball", "ws id cmd");
  expect(msg.view_id == 2, "ws view_id");
}

void test_reject_unknown_topic() {
  const char* json =
      R"({"topic":"not.a.topic","payload":{"command":"view.pan"}})";
  app::cef::BridgeMessage msg;
  expect(!app::cef::detail::parse_bridge_message(json, &msg), "unknown topic");
}

}  // namespace

int main() {
  test_parse_activate_tool();
  test_reject_bad_version();
  test_parse_topic_tool_command();
  test_parse_topic_panel_action();
  test_parse_topic_workspace_ids();
  test_reject_unknown_topic();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("chrome_bridge_test ok\n");
  return 0;
}
