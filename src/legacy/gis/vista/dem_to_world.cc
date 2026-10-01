// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/gis/vista/dem_to_world.h"

#include "gis/vista/world/terrain/dem/dem_raster.h"

namespace render {

gis::Node* seed_dem_height_field_into_world(gis::World* world,
                                            const DemHeightField& dem,
                                            const char* name, int max_edge) {
  // DemRaster is the World/GpuScene authority; DemHeightField is a thin shell.
  return gis::seed_dem_raster_into_world(world, dem.dem_raster(), name,
                                         max_edge);
}

}  // namespace render
