// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/processing/playback_frames.h"

#include <string>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/processing/args_json.h"
#include "plugin/runtime/host/processing/operation_result.h"

#include <rapidjson/document.h>

namespace plugin {

void fill_playback_index_frames(content::PluginHost* host, int frame_count) {
  if (!host) {
    return;
  }
  content::PluginHost::Playback* pb = host->playback();
  if (!pb) {
    return;
  }
  pb->clear();
  const int n = frame_count > 0 ? frame_count : 0;
  for (int i = 0; i < n; ++i) {
    pb->push_frame("{\"index\":" + std::to_string(i) + "}");
  }
  pb->set_index(static_cast<size_t>(n > 0 ? n - 1 : 0));
}

int clamp_frame_index(int index, int frame_count) {
  if (frame_count <= 0) {
    return 0;
  }
  if (index < 0) {
    return 0;
  }
  if (index >= frame_count) {
    return frame_count - 1;
  }
  return index;
}

int parse_frame_index_arg(std::string_view args_json, int frame_count) {
  int index = 0;
  rapidjson::Document args;
  if (parse_args_json(args_json, &args)) {
    args_json_int(args, "index", &index);
  }
  return clamp_frame_index(index, frame_count);
}

void set_playback_frame_index(content::PluginHost* host, int index) {
  if (!host) {
    return;
  }
  if (content::PluginHost::Playback* pb = host->playback()) {
    pb->set_index(static_cast<size_t>(index < 0 ? 0 : index));
  }
}

content::GisDocument* require_gis_document(content::PluginHost* host,
                                           const char* op,
                                           const char* error_token) {
  if (!host) {
    set_operation_result(std::string("{\"error\":\"") +
                         (error_token ? error_token : "no_host") +
                         "\",\"op\":\"" + (op ? op : "") + "\"}");
    return nullptr;
  }
  content::GisDocument* gis = host->gis_document();
  if (!gis) {
    set_operation_result(std::string("{\"error\":\"") +
                         (error_token ? error_token : "no_gis") +
                         "\",\"op\":\"" + (op ? op : "") + "\"}");
    return nullptr;
  }
  return gis;
}

}  // namespace plugin
