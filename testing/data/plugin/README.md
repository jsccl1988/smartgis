# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Plugin product sample fixtures (copied to out/data/plugin/).
# Not the china pack under testing/data/china/ — these are schematic/tiny smokes.
# - world3d_trimesh_sample.xyz → world3d.trimesh_from_xyz
# - world3d_dem_sample.tif → world3d.heightmap_from_raster (tiny 8x8; not china_dem)
# - world3d_pointcloud_sample.las → uncolored LAS fallback for LAS/LAZ smoke
# Prefer colored public DEM sample: out/data/pointcloud_public_sample.txt
#   (china_dem → RGB; rebuild: py -3 testing/data/build_pointcloud_sample.py)
# Rebuild LAS: py -3 testing/data/build_world3d_pointcloud_las.py
# - stormsurge_dem_sample.tif / stormsurge_coast_sample.geojson /
#   stormsurge_tide_sample.csv / stormsurge_impact_sample.geojson
#   → smartgis.stormsurge (P2 overlap stats)
#   Rebuild: py -3 testing/data/build_plugin_stormsurge_samples.py
# Optional China coastal showcase: replace DEM with a real coastal raster
# under out/data/ when available (see build script note).
# - traffic_network_sample.geojson → traffic.cost_path
# - flood_basin_sample.tif → flood.inundate
# - mine_boreholes.csv → mine.load_boreholes / mine.interpolate_stratum
# - geochem/geochem_samples.csv → geochem.analyze / geochem.stats (120 pts)
