// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/product/world3d/scene/look/look.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/scene3d_sink.h"
#include "plugin/runtime/host/processing/operation_result.h"

#include <string>

#include "gtest/gtest.h"

namespace {

TEST(World3dEarthSystem, ParsesLookName) {
  plugin::World3dLook parsed = plugin::World3dLook::kLand;
  EXPECT_TRUE(plugin::parse_world3d_look("earth_system", &parsed));
  EXPECT_EQ(parsed, plugin::World3dLook::kEarthSystem);
  EXPECT_STREQ(plugin::world3d_look_name(plugin::World3dLook::kEarthSystem),
               "earth_system");
  EXPECT_FALSE(plugin::parse_world3d_look("nope", &parsed));
}

TEST(World3dEarthSystem, RefusesWithoutLookBridges) {
  content::PluginHost* host =
      content::create_plugin_host(nullptr, nullptr, nullptr);
  ASSERT_NE(host, nullptr);
  plugin::HostCapabilities caps;
  caps.attach(host);
  ASSERT_TRUE(plugin::register_world3d(host));

  EXPECT_FALSE(host->run_processing("world3d.earth_system", "{}"));
  EXPECT_NE(plugin::operation_result().find("no_scene_device"),
            std::string::npos);
  EXPECT_NE(plugin::operation_result().find("world3d.earth_system"),
            std::string::npos);
}

TEST(World3dEarthSystem, CommandReportsComposedLayers) {
  content::PluginHost* host =
      content::create_plugin_host(nullptr, nullptr, nullptr);
  ASSERT_NE(host, nullptr);
  plugin::HostCapabilities caps;
  caps.attach(host);
  ASSERT_TRUE(plugin::register_world3d(host));

  plugin::Scene3dSink* sink = plugin::scene3d_sink(host);
  ASSERT_NE(sink, nullptr);
  sink->set_look_bridges(
      [](std::string_view mode, std::string* result) {
        plugin::World3dLook look = plugin::World3dLook::kLand;
        if (!plugin::parse_world3d_look(mode, &look) ||
            look != plugin::World3dLook::kEarthSystem) {
          return false;
        }
        if (result) {
          *result = "{\"ok\":true}";
        }
        return true;
      },
      [](float, std::string*) { return true; });

  EXPECT_TRUE(host->run_processing("world3d.earth_system", "{}"));
  const std::string& json = plugin::operation_result();
  EXPECT_NE(json.find("\"mode\":\"earth_system\""), std::string::npos);
  EXPECT_NE(json.find("\"space\":\"high_orbit\""), std::string::npos);
  EXPECT_NE(json.find("\"land\":\"globe_dem\""), std::string::npos);
  EXPECT_NE(json.find("\"sea\":\"globe_terrain\""), std::string::npos);
  EXPECT_NE(json.find("\"planar_ocean\":false"), std::string::npos);
  EXPECT_NE(json.find("\"contour\":false"), std::string::npos);
}

}  // namespace
