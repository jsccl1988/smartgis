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

void test_parse_topic_backend_commands() {
  const char* json =
      R"({"topic":"tool.command","payload":{"command":"view.backend.maplibre"}})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(json, &msg), "backend parse");
  expect(msg.type == app::cef::BridgeType::kActivateTool, "backend type");
  expect(msg.command_id == "view.backend.maplibre", "backend cmd");
  const char* rhi =
      R"({"api_version":1,"type":"ActivateTool","command_id":"view.backend.rhi"})";
  expect(app::cef::detail::parse_bridge_message(rhi, &msg), "rhi parse");
  expect(msg.command_id == "view.backend.rhi", "rhi cmd");
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

void test_parse_pointer_wheel() {
  const char* json =
      R"({"api_version":1,"type":"PointerEvent","kind":"wheel",)"
      R"("x":12,"y":18,"wheel":120})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(json, &msg), "ptr parse");
  expect(msg.type == app::cef::BridgeType::kPointerEvent, "ptr type");
  expect(msg.pointer_kind == "wheel", "ptr kind");
  expect(msg.slot_rect.x == 12 && msg.slot_rect.y == 18, "ptr xy");
  expect(msg.wheel == 120, "ptr wheel");
}

void test_parse_pointer_drag_and_pinch() {
  const char* drag =
      R"({"api_version":1,"type":"PointerEvent","kind":"drag","x":4,"y":5})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(drag, &msg), "drag parse");
  expect(msg.pointer_kind == "drag", "drag kind");
  const char* pinch =
      R"({"topic":"map.gesture","payload":{"kind":"pinch","x":8,"y":9,)"
      R"("scale":1.25}})";
  expect(app::cef::detail::parse_bridge_message(pinch, &msg), "pinch parse");
  expect(msg.type == app::cef::BridgeType::kPointerEvent, "pinch type");
  expect(msg.pointer_kind == "pinch", "pinch kind");
  expect(msg.scale > 1.2f && msg.scale < 1.3f, "pinch scale");
  const char* pan2 =
      R"({"api_version":1,"type":"PointerEvent","kind":"move","x":10,"y":20,)"
      R"("pointer_count":2})";
  expect(app::cef::detail::parse_bridge_message(pan2, &msg), "2finger parse");
  expect(msg.pointer_count == 2, "2finger count");
  const char* hwheel =
      R"({"api_version":1,"type":"PointerEvent","kind":"wheel","x":1,"y":2,)"
      R"("wheel":120,"flags":33554432})";
  expect(app::cef::detail::parse_bridge_message(hwheel, &msg), "hwheel parse");
  expect(msg.wheel == 120, "hwheel wheel");
  expect(msg.flags == 0x02000000u, "hwheel horizontal flag");
}

}  // namespace

int main() {
  test_parse_activate_tool();
  test_reject_bad_version();
  test_parse_topic_tool_command();
  test_parse_topic_panel_action();
  test_parse_topic_workspace_ids();
  test_parse_topic_backend_commands();
  test_reject_unknown_topic();
  test_parse_pointer_wheel();
  test_parse_pointer_drag_and_pinch();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("chrome_bridge_test ok\n");
  return 0;
}
