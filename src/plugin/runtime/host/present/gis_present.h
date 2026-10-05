// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_PRESENT_GIS_PRESENT_H_
#define PLUGIN_RUNTIME_HOST_PRESENT_GIS_PRESENT_H_

#include <string_view>
#include <utility>
#include <vector>

#include "plugin/runtime/host/plugin_host_export.h"

namespace content {
class GisDocument;
}

namespace plugin {

// Shared GisDocument present helpers (Map2d features, Scene3d stand-in mesh,
// style). Product present TUs must not include app/views.
PLUGIN_HOST_EXPORT bool append_map_polygon(
    content::GisDocument* doc,
    const std::vector<std::pair<double, double>>& ring, const char* heat);

PLUGIN_HOST_EXPORT bool append_map_polyline(
    content::GisDocument* doc,
    const std::vector<std::pair<double, double>>& xy, const char* frame_tag);

PLUGIN_HOST_EXPORT bool append_map_polyline(
    content::GisDocument* doc,
    const std::vector<std::pair<double, double>>& xy, const char* frame_tag,
    const char* type_field, const char* heat);

PLUGIN_HOST_EXPORT bool apply_style_json(content::GisDocument* doc,
                                         const char* json);

// Read StyleDocument JSON from |path| and apply via apply_style_json.
PLUGIN_HOST_EXPORT bool apply_style_file(content::GisDocument* doc,
                                         const char* path);

// resolve_resource(plugin_id, relative) then apply_style_file.
PLUGIN_HOST_EXPORT bool apply_style_resource(content::GisDocument* doc,
                                             std::string_view plugin_id,
                                             std::string_view relative);

PLUGIN_HOST_EXPORT bool add_standin_mesh(content::GisDocument* doc,
                                         const char* name, double lon,
                                         double lat, double half_deg);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_PRESENT_GIS_PRESENT_H_
