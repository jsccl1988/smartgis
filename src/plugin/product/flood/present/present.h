// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_FLOOD_PRESENT_PRESENT_H_
#define PLUGIN_FLOOD_PRESENT_PRESENT_H_

namespace content {
class GisDocument;
}

namespace plugin {

bool present_flood_style(content::GisDocument* doc);

// Wet-cell mosaic + optional dry-land terrain. Horizon playback still wraps
// GisScene in GisSceneDocument before calling here.
bool present_flood_mask(content::GisDocument* doc,
                        const unsigned char* mask,
                        int width,
                        int height,
                        const double* geotransform,
                        double water_level,
                        bool rebuild_terrain,
                        bool add_water_standin);

}  // namespace plugin

#endif  // PLUGIN_FLOOD_PRESENT_PRESENT_H_
