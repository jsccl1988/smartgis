// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/atmosphere/panel.h"

#include <memory>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/detail/host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/contribute.h"
#include "plugin/runtime/widgets/present_surface_picker.h"
#include "ui/gis/shell/atmosphere_panel.h"

namespace plugin {
namespace detail {

void wire_atmosphere_panel(ui::views::AtmospherePanel* panel,
                           content::PluginHost* host) {
  if (!panel) {
    return;
  }
  Scene3dSink* sink = scene3d_sink(host);
  panel->set_time_range(0.0, 3600.0);
  panel->set_ocean_checked(false);
  panel->set_cloud_checked(false);
  panel->set_sky_checked(false);
  panel->set_fog_checked(false);
  panel->set_wind_checked(false);

  panel->set_time_change([sink](double t) {
    if (!sink) {
      return;
    }
    sink->set_time_sec(t);
    sink->invalidate();
  });
  auto apply_layers = [panel, sink]() {
    if (!sink) {
      return;
    }
    sink->set_atmosphere(panel->sky_checked(), panel->ocean_checked(),
                         panel->cloud_checked(), panel->fog_checked());
    sink->invalidate();
  };
  panel->set_ocean_change([sink, apply_layers](bool on) {
    if (on && sink) {
      sink->seed_if_empty();
    }
    apply_layers();
  });
  panel->set_cloud_change([sink, apply_layers](bool on) {
    if (on && sink) {
      sink->seed_if_empty();
    }
    apply_layers();
  });
  panel->set_sky_change([sink, apply_layers](bool on) {
    if (on && sink) {
      sink->seed_if_empty();
    }
    apply_layers();
  });
  panel->set_fog_change([sink, apply_layers](bool on) {
    if (on && sink) {
      sink->seed_if_empty();
    }
    apply_layers();
  });
  panel->set_wind_change([sink](bool on) {
    if (!sink) {
      return;
    }
    sink->set_wind_overlay(on);
    sink->invalidate();
  });
}

bool contribute_atmosphere_panel(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  const content::DockContribution dock = {"world3d.atmosphere", "Atmosphere",
                                          "right"};
  if (!host->contribute_dock(
          kWorld3dPluginId, dock, [host](content::PluginHost*) {
            auto panel = std::make_unique<ui::views::AtmospherePanel>();
            wire_atmosphere_panel(panel.get(), host);
            auto wrapped = wrap_with_present_surface(host, std::move(panel));
            if (ShellUiSink* ui = shell_ui(host)) {
              (void)ui->mount_inspector("world3d.atmosphere", "Atmosphere",
                                        std::move(wrapped));
            }
          })) {
    return false;
  }
  return contribute_command_aliases(
      host, kWorld3dPluginId, {{"world3d.atmosphere_panel", "Atmosphere"}},
      "view", [host](const tool::CommandArgs&) {
        return host && host->open_dock("world3d.atmosphere");
      });
}

}  // namespace detail
}  // namespace plugin
