// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_GLOBE_PASS_H_
#define EFFECT_ATMOSPHERE_GLOBE_PASS_H_

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

namespace effect {
namespace atmosphere {

// Knobs for the Google-Earth-style DEM globe (geocentric Y-up sphere).
struct GlobeDrawParams {
  float radius = 1.0f;
  // Meters → radius units. Splash exaggeration so near-earth skim reads DEM
  // relief (Everest ≈ 0.10 R) without piercing orbit floor ~1.08.
  float height_scale = 9.5e-6f;
  // Dense UV sphere — near-earth flythrough needs fine lon/lat tessellation.
  int lon_slices = 256;
  int lat_slices = 128;
  float sun_x = 0.0f;
  float sun_y = 0.7071f;
  float sun_z = 0.7071f;
  float ambient = 0.28f;
  float intensity = 1.45f;
};

// UV sphere with optional DEM height displace + equirect albedo.
// Host (AtmosphereSession) loads DemRaster / imagery and calls set_dem_surface.
// When the DEM envelope is regional (e.g. china_dem), the rest of the sphere
// stays at radius with ocean albedo — proves geometry until global_dem.tif.
class GlobePass {
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
  void apply_dem_hillshade();
  float sample_height(double lon, double lat) const;
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

  // Equirect RGBA uploaded to GPU (ocean + DEM window).
  std::vector<uint8_t> equirect_;
  int equirect_w_ = 0;
  int equirect_h_ = 0;

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

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_GLOBE_PASS_H_
