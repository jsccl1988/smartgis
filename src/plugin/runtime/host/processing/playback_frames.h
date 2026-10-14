// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_PROCESSING_PLAYBACK_FRAMES_H_
#define PLUGIN_RUNTIME_HOST_PROCESSING_PLAYBACK_FRAMES_H_

#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace content {
class GisDocument;
class PluginHost;
}  // namespace content

namespace plugin {

// Shared playback helpers for flood / traffic / stormsurge / orthogrid, etc.

// Clears playback and pushes {"index":i} for i in [0, frame_count), then
// selects the last frame (or 0 when empty). No-op when host/playback is null.
PLUGIN_HOST_EXPORT void fill_playback_index_frames(content::PluginHost* host,
                                                   int frame_count);

// Clamps |index| into [0, frame_count-1]; empty/non-positive count → 0.
PLUGIN_HOST_EXPORT int clamp_frame_index(int index, int frame_count);

// Reads optional args.index (default 0) and clamps to |frame_count|.
PLUGIN_HOST_EXPORT int parse_frame_index_arg(std::string_view args_json,
                                             int frame_count);

// Sets Playback::set_index when host has a playback sink.
PLUGIN_HOST_EXPORT void set_playback_frame_index(content::PluginHost* host,
                                                 int index);

// Returns host->gis_document() or nullptr after set_operation_result with
// {"error":"|error_token|","op":"|op|"}.
PLUGIN_HOST_EXPORT content::GisDocument* require_gis_document(
    content::PluginHost* host,
    const char* op,
    const char* error_token);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_PROCESSING_PLAYBACK_FRAMES_H_
