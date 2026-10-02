// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/panels/atmosphere_chrome.h"
#include "app/views/shell/ui/browser_view.h"

#include "app/views/shell/browser/browser.h"

#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "ui/gis/shell/atmosphere_panel.h"

namespace app {

// Atmosphere panel wiring (ocean / cloud / sky / fog / wind).

// Atmosphere inspector panel chrome wire.
AtmosphereChrome::AtmosphereChrome(BrowserView* host) : host_(host) {}

void AtmosphereChrome::wire_atmosphere_panel() {
  if (!host_->atmosphere_panel_) {
    return;
  }
  host_->atmosphere_panel_->set_time_range(0.0, 3600.0);
  // Keep scrub at 0 during chrome init. Reading atmosphere_session().time_sec()
  // here forced Label::set_text while the shell heap was still settling and
  // AVd under FAST_FAIL_INVALID_ARG on string deallocate.
  host_->atmosphere_panel_->set_ocean_checked(false);
  host_->atmosphere_panel_->set_cloud_checked(false);
  host_->atmosphere_panel_->set_sky_checked(false);
  host_->atmosphere_panel_->set_fog_checked(false);
  host_->atmosphere_panel_->set_wind_checked(false);

  host_->atmosphere_panel_->set_time_change([this](double t) {
    host_->browser_->scene3d()->atmosphere_session().set_time_sec(t);
    host_->invalidate_map_overlays();
  });
  auto seed_if_empty = [this]() {
    const auto* env = host_->browser_->scene3d()->atmosphere_session().environment();
    if (!env || env->field_store().layer_count() == 0) {
      host_->browser_->scene3d()->atmosphere_session().seed_procedural();
    }
  };
  host_->atmosphere_panel_->set_ocean_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    host_->browser_->scene3d()->atmosphere_session().set_ocean_enabled(on);
    host_->invalidate_map_overlays();
  });
  host_->atmosphere_panel_->set_cloud_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    host_->browser_->scene3d()->atmosphere_session().set_cloud_enabled(on);
    host_->invalidate_map_overlays();
  });
  host_->atmosphere_panel_->set_sky_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    host_->browser_->scene3d()->atmosphere_session().set_sky_enabled(on);
    host_->invalidate_map_overlays();
  });
  host_->atmosphere_panel_->set_fog_change([this, seed_if_empty](bool on) {
    if (on) {
      seed_if_empty();
    }
    host_->browser_->scene3d()->atmosphere_session().set_fog_enabled(on);
    host_->invalidate_map_overlays();
  });
  host_->atmosphere_panel_->set_wind_change([this](bool on) {
    host_->browser_->scene3d()->atmosphere_session().set_wind_overlay_enabled(on);
    host_->invalidate_map_overlays();
  });
}


}  // namespace app
