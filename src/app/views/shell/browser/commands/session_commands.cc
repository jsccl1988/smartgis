// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/processing/ops_runner.h"
#include "ui/views/gis/panel/atmosphere_panel.h"
#include "ui/views/gis/panel/processing_panel.h"

namespace app {
namespace {

std::string m2_json_path(std::string path) {
  for (char& c : path) {
    if (c == '\\') {
      c = '/';
    }
  }
  return path;
}

}  // namespace

bool Browser::run_m2_self_test_hooks(std::string* err) {
  auto fail = [&](const char* msg) {
    if (err) {
      *err = msg;
    }
    return false;
  };
  ui::views::ProcessingPanel* panel = ui_ ? ui_->processing_panel() : nullptr;
  if (!panel || panel->operator_count() < 10) {
    return fail("m2: processing panel lists < 10 operators");
  }
  if (!panel->select_id("native.buffer") || !panel->select_id("native.clip")) {
    return fail("m2: native.buffer/clip missing from panel");
  }
  if (!plugins_ || !plugins_->host()) {
    return fail("m2: plugin host missing");
  }

  // Self-test uses a tiny square fixture (not china_city): ProcessingPool
  // enqueue reports success before the worker finishes, and buffering a
  // prefecture MultiPolygon is too heavy for --self-test.
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path();
  const fs::path in = dir / "smartgis_m2_hook_in.geojson";
  const fs::path out_buf = dir / "smartgis_m2_hook_buffer.geojson";
  const fs::path clip = dir / "smartgis_m2_hook_clip.geojson";
  const fs::path out_clip = dir / "smartgis_m2_hook_clip_out.geojson";
  {
    std::ofstream out(in);
    if (!out) {
      return fail("m2: write fixture failed");
    }
    out << "{\"type\":\"FeatureCollection\",\"features\":[{"
           "\"type\":\"Feature\",\"properties\":{},\"geometry\":{"
           "\"type\":\"Polygon\",\"coordinates\":[[[0,0],[2,0],[2,2],[0,2],"
           "[0,0]]]}}]}";
  }
  {
    std::ofstream out(clip);
    if (!out) {
      return fail("m2: clip fixture write failed");
    }
    out << "{\"type\":\"FeatureCollection\",\"features\":[{"
           "\"type\":\"Feature\",\"properties\":{},\"geometry\":{"
           "\"type\":\"Polygon\",\"coordinates\":[[[1,1],[3,1],[3,3],[1,3],"
           "[1,1]]]}}]}";
  }

  const std::string buf_args =
      std::string("{\"input\":\"") + m2_json_path(in.string()) +
      "\",\"output\":\"" + m2_json_path(out_buf.string()) +
      "\",\"distance\":0.5}";
  if (!plugin::run_builtin_op("native.buffer", buf_args)) {
    return fail("m2: run native.buffer failed");
  }
  if (!document_.open_path(out_buf.string()) || document_.feature_count() < 1) {
    return fail("m2: buffer write-back produced no features");
  }

  const std::string clip_args =
      std::string("{\"input\":\"") + m2_json_path(in.string()) +
      "\",\"output\":\"" + m2_json_path(out_clip.string()) + "\",\"clip\":\"" +
      m2_json_path(clip.string()) + "\"}";
  if (!plugin::run_builtin_op("native.clip", clip_args)) {
    return fail("m2: run native.clip failed");
  }
  if (!document_.open_path(out_clip.string()) || document_.feature_count() < 1) {
    return fail("m2: clip write-back produced no features");
  }

  if (ui_) {
    ui_->sync_catalog_from_scene();
    ui_->sync_inspectors_from_scene();
    ui_->invalidate_map_overlays();
  }
  if (err) {
    err->clear();
  }
  return true;
}

bool Browser::apply_atmosphere_fields(std::string_view spec) {
  const bool ok = scene3d_.load_atmosphere_fields(spec);
  ui::views::AtmospherePanel* panel = ui_ ? ui_->atmosphere_panel() : nullptr;
  if (ok && panel) {
    double t_min = 0.0;
    double t_max = 3600.0;
    if (auto* env = scene3d_.atmosphere()) {
      static const gis::atmosphere::FieldChannel kRangeOrder[] = {
          gis::atmosphere::FieldChannel::kWaveHs,
          gis::atmosphere::FieldChannel::kCloudCover,
          gis::atmosphere::FieldChannel::kWindU,
          gis::atmosphere::FieldChannel::kWindV,
          gis::atmosphere::FieldChannel::kWaveDir,
          gis::atmosphere::FieldChannel::kCloudBase,
          gis::atmosphere::FieldChannel::kCloudTop,
          gis::atmosphere::FieldChannel::kSeaMask,
      };
      for (gis::atmosphere::FieldChannel ch : kRangeOrder) {
        if (env->timed_field_range(ch, &t_min, &t_max)) {
          break;
        }
      }
    }
    if (t_max < t_min) {
      std::swap(t_min, t_max);
    }
    if (t_max <= t_min) {
      t_max = t_min + 1.0;
    }
    panel->set_time_range(t_min, t_max);
    panel->set_time_sec(scene3d_.time_sec());
  }
  if (ui_) {
    if (ok) {
      ui_->invalidate_map_overlays();
      ui_->set_status_message("Atmosphere fields loaded");
    } else {
      ui_->set_status_message("Atmosphere fields load failed");
    }
  }
  return ok;
}

}  // namespace app
