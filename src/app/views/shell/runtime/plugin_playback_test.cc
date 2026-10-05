// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/plugin_playback.h"

#include <cstdio>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_adopt_and_clamp() {
  app::PluginPlayback s;
  s.adopt_host_frames("smartgis.traffic", "traffic.present_frame", 4);
  expect(s.plugin_id() == "smartgis.traffic", "plugin id");
  expect(s.present_frame_id() == "traffic.present_frame", "present id");
  expect(s.label() == "traffic", "label");
  expect(s.frame_count() == 4, "frame_count 4");
  expect(s.frame_index() == 3, "index last");
  expect(s.set_frame_index(0), "set frame 0");
  expect(s.frame_index() == 0, "index 0");
  expect(s.set_frame_index(99), "clamp high");
  expect(s.frame_index() == 3, "clamped to 3");
  expect(s.playback_json().find("\"traffic.present_frame\"") != std::string::npos,
         "playback json");
}

void test_empty() {
  app::PluginPlayback s;
  expect(!s.set_frame_index(0), "empty set_frame fails");
  expect(s.label() == "session", "empty label");
}

void test_host_playback_tick() {
  app::PluginPlayback s;
  int last_index = -1;
  int last_count = -1;
  int ticks = 0;
  s.bind_host_playback([&](int index, int count, bool /*rebuild*/) {
    last_index = index;
    last_count = count;
    ++ticks;
  });
  expect(last_count == 0, "bind empty count");
  s.adopt_host_frames("smartgis.flood", "flood.present_frame", 4);
  expect(last_count == 4, "host count 4");
  expect(last_index == 3, "host index last");
  expect(s.set_frame_index(1), "set 1");
  expect(last_index == 1, "host ticks index");
  expect(ticks >= 3, "bind + adopt + set");
}

}  // namespace

int main() {
  test_adopt_and_clamp();
  test_empty();
  test_host_playback_tick();
  if (g_fails != 0) {
    std::fprintf(stderr, "plugin_playback_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::printf("plugin_playback_test: ok\n");
  return 0;
}
