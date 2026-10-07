// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_GLOBE_GLOBE_PASS_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_GLOBE_GLOBE_PASS_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace render {
namespace rhi {
class Buffer;
class CommandList;
class Device;
class Pipeline;
class Texture;
struct CameraMatrices;
}  // namespace rhi
}  // namespace render

#include "vista/vista_export.h"

namespace vista {

// Knobs for the Google-Earth-style DEM globe (geocentric Y-up sphere).
struct GlobeDrawParams {
  float radius = 2.0f;
  // Meters → radius units. Mild relief so near-earth skim reads DEM without
  // chrome cliffs; fly distances scale with radius.
  float height_scale = 1.8e-6f;
  // Dense UV sphere — near-earth flythrough needs fine lon/lat tessellation.
  int lon_slices = 256;
  int lat_slices = 128;
  float sun_x = 0.0f;
  float sun_y = 0.7071f;
  float sun_z = 0.7071f;
  float ambient = 0.48f;
  float intensity = 0.38f;
};

// UV sphere with optional DEM height displace + equirect albedo.
// Host (AtmosphereSession) loads DemRaster / imagery and calls set_dem_surface.
// When the DEM envelope is regional (e.g. china_dem), the rest of the sphere
// stays at radius with ocean albedo — proves geometry until global_dem.tif.
// Near-earth flythrough may attach a china_dem overlay via set_detail_dem_surface.
class VISTA_EXPORT GlobePass {
 public:
  GlobePass();
  ~GlobePass();

  GlobePass(const GlobePass&) = delete;
  GlobePass& operator=(const GlobePass&) = delete;

  void set_params(const GlobeDrawParams& params);
  const GlobeDrawParams& params() const { return params_; }

  void set_sun_direction(float x, float y, float z);

  // Lon/lat height grid (degrees) + optional RGBA8 albedo (DEM bake or imagery).
  // Empty albedo → procedural ocean / land tint from heights.
  void set_dem_surface(double min_lon, double min_lat, double max_lon,
                       double max_lat, int cols, int rows,
                       const float* heights_m, std::size_t height_count,
                       const uint8_t* albedo_rgba, int tex_w, int tex_h);

  // Regional high-res DEM (china_dem) blended over the global grid. |blend|
  // 0 = global only; 1 = full overlay inside the China envelope (soft edges).
  void set_detail_dem_surface(double min_lon, double min_lat, double max_lon,
                              double max_lat, int cols, int rows,
                              const float* heights_m, std::size_t height_count,
                              const uint8_t* albedo_rgba, int tex_w, int tex_h);
  void set_detail_blend(float blend);
  float detail_blend() const { return detail_blend_; }
  bool has_detail_surface() const { return !detail_heights_.empty(); }

  // Origin-like overlay on the China DEM window: jet elevation surface
  // and/or isoline curves. Default both on so near-earth china_dem reads
  // as a scientific height field rather than only satellite/hypsometric.
  void set_elevation_overlay(bool surface, bool curves);
  bool elevation_surface_overlay() const { return elevation_surface_; }
  bool elevation_curve_overlay() const { return elevation_curves_; }
  bool has_elevation_overlay() const { return !elev_overlay_.empty(); }

  // Build far / mid / near CPU meshes into the LOD cache before fly-in.
  void prewarm_mesh_lods();

  bool has_surface() const { return surface_ready_; }
  bool dem_is_global() const { return dem_is_global_; }

  // DEM height in meters at lon/lat (degrees); 0 outside the envelope.
  float height_meters(double lon_deg, double lat_deg) const {
    return sample_height(lon_deg, lat_deg);
  }
  // Geocentric radius at lon/lat including height_scale displace.
  float surface_radius(double lon_deg, double lat_deg) const;

  // Record opaque textured globe into an open CommandList. Does not close.
  bool record(render::rhi::Device* device, render::rhi::CommandList* list,
              uint32_t width, uint32_t height,
              const render::rhi::CameraMatrices* camera);

  render::rhi::Pipeline* pipeline() const { return pipeline_; }

  void release();

 private:
  bool ensure_pipeline(render::rhi::Device* device);
  bool ensure_gpu(render::rhi::Device* device);
  void destroy_pipeline();
  void rebuild_mesh();
  void rebuild_equirect_albedo();
  void rebuild_elevation_overlay();
  void stamp_elevation_overlay();
  void apply_dem_hillshade();
  float sample_height(double lon, double lat) const;
  float sample_detail_height(double lon, double lat) const;
  // Outward DEM surface normal at lon/lat (degrees); geocentric when flat.
  void sample_normal(double lon_deg, double lat_deg, float* nx, float* ny,
                     float* nz) const;

  GlobeDrawParams params_;
  bool surface_ready_ = false;
  bool dem_is_global_ = false;
  bool mesh_dirty_ = true;
  bool albedo_dirty_ = true;

  double dem_min_lon_ = -180.0;
  double dem_min_lat_ = -90.0;
  double dem_max_lon_ = 180.0;
  double dem_max_lat_ = 90.0;
  int dem_cols_ = 0;
  int dem_rows_ = 0;
  std::vector<float> dem_heights_;
  std::vector<uint8_t> dem_albedo_;
  int dem_tex_w_ = 0;
  int dem_tex_h_ = 0;

  // Near-earth China overlay (optional).
  double detail_min_lon_ = 73.0;
  double detail_min_lat_ = 18.0;
  double detail_max_lon_ = 135.0;
  double detail_max_lat_ = 54.0;
  int detail_cols_ = 0;
  int detail_rows_ = 0;
  std::vector<float> detail_heights_;
  std::vector<uint8_t> detail_albedo_;
  int detail_tex_w_ = 0;
  int detail_tex_h_ = 0;
  float detail_blend_ = 0.f;

  bool elevation_surface_ = true;
  bool elevation_curves_ = true;
  std::vector<uint8_t> elev_overlay_;
  int elev_overlay_w_ = 0;
  int elev_overlay_h_ = 0;
  double elev_min_lon_ = 73.0;
  double elev_min_lat_ = 18.0;
  double elev_max_lon_ = 135.0;
  double elev_max_lat_ = 54.0;

  // Equirect RGBA uploaded to GPU (ocean + DEM window).
  std::vector<uint8_t> equirect_;
  int equirect_w_ = 0;
  int equirect_h_ = 0;
  int gpu_equirect_w_ = 0;
  int gpu_equirect_h_ = 0;
  uint32_t gpu_vb_bytes_ = 0;

  std::vector<float> positions_;  // xyz + normal + uv interleaved (8 floats)
  std::vector<uint32_t> indices_;

  render::rhi::Device* device_ = nullptr;
  render::rhi::Device* pipeline_device_ = nullptr;
  render::rhi::Pipeline* pipeline_ = nullptr;
  render::rhi::Buffer* vertex_buffer_ = nullptr;
  render::rhi::Buffer* index_buffer_ = nullptr;
  render::rhi::Texture* albedo_tex_ = nullptr;
  uint32_t index_count_ = 0;
};

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_GLOBE_GLOBE_PASS_H_
