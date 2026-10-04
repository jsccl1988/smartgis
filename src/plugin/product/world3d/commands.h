// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_COMMANDS_H_
#define PLUGIN_WORLD3D_COMMANDS_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ogr_geometry.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Views map commits the loaded trimesh/heightmap. Unset writer keeps no_map_seam.
using World3dSurfaceWriter = std::function<bool(
    const double* xyz, int point_count, const int* triangles,
    int triangle_count, const char* op)>;
void set_world3d_surface_writer(World3dSurfaceWriter writer);

// Scene-device write seam for former model3d ops + True Earth browse.
// App shell installs when a real scene / map stand-in exists; unset callbacks
// keep no_scene_device.
struct World3dSceneWriter {
  std::function<bool(const std::string& path)> add_pointcloud;
  // Optional in-memory attach (PDAL / preloaded buffers). xyz interleaved;
  // rgba may be null or 4 * point_count (RGBA8).
  std::function<bool(const float* xyz, int point_count, const uint8_t* rgba)>
      add_pointcloud_xyz;
  std::function<bool()> add_sphere;
  std::function<bool()> add_water;
  std::function<bool()> add_terrain_heightmap;
  std::function<bool()> add_terrain_trimesh;
  std::function<bool()> layer_points_to_3d;
  std::function<bool()> layer_lines_to_3d;
  std::function<bool()> layer_polygons_to_3d;
  std::function<bool()> create_trimesh_from_active_layer;
  // Google-Earth-class MVP: Scene3D tab + China DEM + atmosphere product
  // defaults. Optional empty path for attach uses shipped m3 city fixture.
  std::function<bool()> open_earth;
  // Local lon/lat extent reframe + orbit distance (span_deg half-box).
  std::function<bool(double lon, double lat, float distance, double span_deg)>
      fly_to;
  // Attach 3D Tiles tileset JSON (city fixture or user path).
  std::function<bool(const std::string& tileset_json_path)> attach_tileset;
  // Global / custom DEM GeoTIFF. Empty path → resolve default locations
  // (out/data/global_dem.tif, plugins/world3d/data/, else china stand-in).
  // Returns false only on hard failure; missing global file may still succeed
  // with china_dem as documented stand-in (result JSON carries source).
  std::function<bool(const std::string& dem_path, std::string* result_json)>
      load_global_dem;
  // Satellite cloud cover field. Empty path → procedural atmosphere clouds.
  // Non-empty → AtmosphereSession::load_fields("|path|:cloud_cover").
  std::function<bool(const std::string& imagery_path, bool enabled,
                     std::string* result_json)>
      set_satellite_cloud;
  // Explicit atmosphere pass toggles (sky / ocean / cloud / fog).
  std::function<bool(bool sky, bool ocean, bool cloud, bool fog)>
      set_atmosphere;
};

void set_world3d_scene_writer(World3dSceneWriter writer);

// Solved mesh + dual orthogonality heat fields for MapScene commit.
struct OrthogridMeshCommit {
  int nx = 0;
  int ny = 0;
  const double* xs = nullptr;
  const double* ys = nullptr;
  // Cell |90-theta|; size (nx-1)*(ny-1). Optional.
  const float* cell_orth = nullptr;
  int raster_w = 0;
  int raster_h = 0;
  double raster_min_x = 0.0;
  double raster_min_y = 0.0;
  double raster_max_x = 0.0;
  double raster_max_y = 0.0;
  // Axis-aligned heat samples; size raster_w*raster_h. Optional.
  const float* raster_orth = nullptr;
  // Intermediate grids (Laplace + each elliptic step). Optional.
  int frame_count = 0;
  const std::vector<double>* frame_xs = nullptr;
  const std::vector<double>* frame_ys = nullptr;
};

using OrthogridMeshWriter = std::function<bool(const OrthogridMeshCommit&)>;
void set_orthogrid_mesh_writer(OrthogridMeshWriter writer);
bool publish_orthogrid_mesh(const OrthogridMeshCommit& commit);

// Digitizing arms a flag (0..3). The next linestring the shell notes is
// stored; save_boundary writes gridbnd text.
void arm_grid_boundary(int flag);
bool grid_boundary_armed();
bool note_grid_boundary(const double* xy, size_t count);

void set_orthogrid_elliptic_iters(int n);
int orthogrid_elliptic_iters();

// Solved volume mesh for Scene3D / analysis commit.
struct HexGridCommit {
  const OGRMultiPoint* nodes = nullptr;
  int nx = 0;
  int ny = 0;
  int nz = 0;
  const float* cell_orth = nullptr;
  int cell_orth_count = 0;
};

using HexGridWriter = std::function<bool(const HexGridCommit&)>;
void set_hex_grid_writer(HexGridWriter writer);
bool publish_hex_grid(const HexGridCommit& commit);

// Host façade: wires DEM, True-Earth scene, 2D orthogrid, and 3D hex layers.
// Command ids stay baogrid.* / orthogrid.* / orthogrid3d.* / model3d.* /
// world3d.* for AM, harness, and host_test.
bool register_world3d(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_COMMANDS_H_
