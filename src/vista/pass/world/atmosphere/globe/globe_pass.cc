// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/atmosphere/globe/globe_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "vista/component/world/atmosphere/detail/math.h"
#include "vista/pass/world/atmosphere/detail/mesh.h"
#include "vista/pass/world/atmosphere/detail/raster.h"
#include "vista/pass/world/atmosphere/globe/constants.h"
#include "vista/pass/world/atmosphere/globe/hlsl.h"
#include "vista/terrain/dem/dem_contour.h"
#include "render/rhi/rhi.h"

namespace vista {
namespace {

constexpr uint32_t kGlobeConstantSlot = 1;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int kGlobeMeshMaxLon = 1024;
constexpr int kGlobeMeshMaxLat = 512;

struct MeshLodCache {
  int nlon = 0;
  int nlat = 0;
  int blend_q = -1;
  float radius = 0.f;
  float height_scale = 0.f;
  std::vector<float> positions;
  std::vector<uint32_t> indices;
};

std::mutex g_mesh_lod_mu;
std::unordered_map<const GlobePass*, std::vector<MeshLodCache>> g_mesh_lod;

render::rhi::GraphicsPipelineDesc globe_graphics_desc() {
  // CameraCB b0 (view/proj via camera_slot) + GlobeCB b1 (lit). Matches
  // sat_cloud / flat DEM so FlyCube always uploads orbit matrices.
  static constexpr render::rhi::BindingSlot kBindings[] = {
      {.slot = 0,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kVertex,
       .size_bytes = 128,
       .hlsl_name = "CameraCB"},
      {.slot = kGlobeConstantSlot,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = sizeof(GlobeConstants),
       .hlsl_name = "GlobeCB"},
      {.slot = 0,
       .kind = render::rhi::BindingKind::kSrv,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = 0,
       .hlsl_name = "albedo_tex"},
      {.slot = 0,
       .kind = render::rhi::BindingKind::kSampler,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = 0,
       .hlsl_name = "linear_sampler"},
  };
  render::rhi::GraphicsPipelineDesc desc;
  desc.vertex.hlsl = kVsGlobe;
  desc.pixel.hlsl = kPsGlobe;
  desc.vertex_layout = render::rhi::VertexLayout::kPositionNormalUv;
  desc.bindings = kBindings;
  desc.binding_count = sizeof(kBindings) / sizeof(kBindings[0]);
  desc.blend = render::rhi::BlendMode::kOpaque;
  // Color-only path: depth_write/test PSOs need a DS-attached pass; a
  // format mismatch silently skips DrawIndexed (sky-only HWND).
  desc.compile_depth_off = true;
  desc.compile_depth_write = true;
  desc.compile_depth_test = true;
  desc.camera_slot = 0;
  return desc;
}

void lonlat_to_xyz(float lon_rad, float lat_rad, float radius, float* x,
                   float* y, float* z) {
  const float cl = std::cos(lat_rad);
  *x = radius * cl * std::sin(lon_rad);
  *y = radius * std::sin(lat_rad);
  *z = radius * cl * std::cos(lon_rad);
}

}  // namespace

GlobePass::GlobePass() = default;

GlobePass::~GlobePass() {
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  release();
}

void GlobePass::set_params(const GlobeDrawParams& params) {
  if (params_.radius != params.radius ||
      params_.height_scale != params.height_scale ||
      params_.lon_slices != params.lon_slices ||
      params_.lat_slices != params.lat_slices) {
    mesh_dirty_ = true;
  }
  // height_scale changes DEM displace → normals + mesh + albedo edge.
  if (params_.height_scale != params.height_scale) {
    albedo_dirty_ = true;
  }
  params_ = params;
  detail::normalize3(&params_.sun_x, &params_.sun_y, &params_.sun_z);
}

void GlobePass::set_sun_direction(float x, float y, float z) {
  params_.sun_x = x;
  params_.sun_y = y;
  params_.sun_z = z;
  detail::normalize3(&params_.sun_x, &params_.sun_y, &params_.sun_z);
  // PS Lambert reads sun from GlobeCB — no albedo rebake.
}

void GlobePass::set_dem_surface(double min_lon, double min_lat, double max_lon,
                                double max_lat, int cols, int rows,
                                const float* heights_m,
                                std::size_t height_count,
                                const uint8_t* albedo_rgba, int tex_w,
                                int tex_h) {
  surface_ready_ = false;
  dem_heights_.clear();
  dem_albedo_.clear();
  dem_cols_ = 0;
  dem_rows_ = 0;
  dem_tex_w_ = 0;
  dem_tex_h_ = 0;
  if (!heights_m || cols < 2 || rows < 2 || max_lon <= min_lon ||
      max_lat <= min_lat) {
    rebuild_elevation_overlay();
    return;
  }
  const std::size_t need =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  if (height_count < need) {
    rebuild_elevation_overlay();
    return;
  }
  dem_min_lon_ = min_lon;
  dem_min_lat_ = min_lat;
  dem_max_lon_ = max_lon;
  dem_max_lat_ = max_lat;
  dem_cols_ = cols;
  dem_rows_ = rows;
  dem_heights_.assign(heights_m, heights_m + need);
  dem_is_global_ = (min_lon <= -170.0 && max_lon >= 170.0 && min_lat <= -80.0 &&
                    max_lat >= 80.0);
  if (albedo_rgba && tex_w > 0 && tex_h > 0) {
    const std::size_t nbytes =
        static_cast<std::size_t>(tex_w) * static_cast<std::size_t>(tex_h) * 4u;
    dem_albedo_.assign(albedo_rgba, albedo_rgba + nbytes);
    dem_tex_w_ = tex_w;
    dem_tex_h_ = tex_h;
  }
  detail_heights_.clear();
  detail_albedo_.clear();
  detail_cols_ = 0;
  detail_rows_ = 0;
  detail_tex_w_ = 0;
  detail_tex_h_ = 0;
  detail_blend_ = 0.f;
  surface_ready_ = true;
  mesh_dirty_ = true;
  albedo_dirty_ = true;
  rebuild_elevation_overlay();
}

void GlobePass::set_detail_dem_surface(double min_lon, double min_lat,
                                       double max_lon, double max_lat, int cols,
                                       int rows, const float* heights_m,
                                       std::size_t height_count,
                                       const uint8_t* albedo_rgba, int tex_w,
                                       int tex_h) {
  detail_heights_.clear();
  detail_albedo_.clear();
  detail_cols_ = 0;
  detail_rows_ = 0;
  detail_tex_w_ = 0;
  detail_tex_h_ = 0;
  if (!heights_m || cols < 2 || rows < 2 || max_lon <= min_lon ||
      max_lat <= min_lat) {
    mesh_dirty_ = true;
    albedo_dirty_ = true;
    rebuild_elevation_overlay();
    return;
  }
  const std::size_t need =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  if (height_count < need) {
    rebuild_elevation_overlay();
    return;
  }
  detail_min_lon_ = min_lon;
  detail_min_lat_ = min_lat;
  detail_max_lon_ = max_lon;
  detail_max_lat_ = max_lat;
  detail_cols_ = cols;
  detail_rows_ = rows;
  detail_heights_.assign(heights_m, heights_m + need);
  if (albedo_rgba && tex_w > 0 && tex_h > 0) {
    const std::size_t nbytes =
        static_cast<std::size_t>(tex_w) * static_cast<std::size_t>(tex_h) * 4u;
    detail_albedo_.assign(albedo_rgba, albedo_rgba + nbytes);
    detail_tex_w_ = tex_w;
    detail_tex_h_ = tex_h;
  }
  mesh_dirty_ = true;
  albedo_dirty_ = true;
  rebuild_elevation_overlay();
}

void GlobePass::set_detail_blend(float blend) {
  const float w = (std::max)(0.f, (std::min)(1.f, blend));
  const float q = std::round(w * 10.f) * 0.1f;
  if (std::fabs(q - detail_blend_) < 0.01f) {
    return;
  }
  detail_blend_ = q;
  mesh_dirty_ = true;
  albedo_dirty_ = true;
}

void GlobePass::set_elevation_overlay(bool surface, bool curves) {
  if (elevation_surface_ == surface && elevation_curves_ == curves) {
    return;
  }
  elevation_surface_ = surface;
  elevation_curves_ = curves;
  rebuild_elevation_overlay();
  albedo_dirty_ = true;
}

void GlobePass::rebuild_elevation_overlay() {
  elev_overlay_.clear();
  elev_overlay_w_ = 0;
  elev_overlay_h_ = 0;
  if (!elevation_surface_ && !elevation_curves_) {
    return;
  }
  const float* heights = nullptr;
  int cols = 0;
  int rows = 0;
  if (!detail_heights_.empty() && detail_cols_ >= 2 && detail_rows_ >= 2) {
    heights = detail_heights_.data();
    cols = detail_cols_;
    rows = detail_rows_;
    elev_min_lon_ = detail_min_lon_;
    elev_min_lat_ = detail_min_lat_;
    elev_max_lon_ = detail_max_lon_;
    elev_max_lat_ = detail_max_lat_;
  } else if (!dem_is_global_ && !dem_heights_.empty() && dem_cols_ >= 2 &&
             dem_rows_ >= 2) {
    heights = dem_heights_.data();
    cols = dem_cols_;
    rows = dem_rows_;
    elev_min_lon_ = dem_min_lon_;
    elev_min_lat_ = dem_min_lat_;
    elev_max_lon_ = dem_max_lon_;
    elev_max_lat_ = dem_max_lat_;
  } else {
    return;
  }
  if (!bake_elevation_overlay_rgba(heights, cols, rows, elevation_surface_,
                                   elevation_curves_, 0.f, &elev_overlay_)) {
    elev_overlay_.clear();
    return;
  }
  elev_overlay_w_ = cols;
  elev_overlay_h_ = rows;
}

namespace {

// Soft falloff near the regional DEM envelope (avoids hard China rectangle).
float dem_edge_fade(double lon, double lat, double lon0, double lat0,
                    double lon1, double lat1) {
  if (lon1 <= lon0 || lat1 <= lat0) {
    return 0.f;
  }
  if (lon < lon0 || lon > lon1 || lat < lat0 || lat > lat1) {
    return 0.f;
  }
  const double u = (lon - lon0) / (lon1 - lon0);
  const double v = (lat - lat0) / (lat1 - lat0);
  const double edge = (std::min)((std::min)(u, 1.0 - u), (std::min)(v, 1.0 - v));
  // ~10% border → smoothstep into full land.
  constexpr double kBorder = 0.10;
  const double t = edge / kBorder;
  if (t >= 1.0) {
    return 1.f;
  }
  if (t <= 0.0) {
    return 0.f;
  }
  const float s = static_cast<float>(t);
  return s * s * (3.f - 2.f * s);
}

float sample_grid_height(const std::vector<float>& heights, int cols, int rows,
                         double min_lon, double min_lat, double max_lon,
                         double max_lat, double lon, double lat) {
  if (heights.empty() || cols < 2 || rows < 2) {
    return 0.f;
  }
  if (lon < min_lon || lon > max_lon || lat < min_lat || lat > max_lat) {
    return 0.f;
  }
  const double u = (lon - min_lon) / (max_lon - min_lon);
  const double v = (max_lat - lat) / (max_lat - min_lat);
  const double fx = u * static_cast<double>(cols - 1);
  const double fy = v * static_cast<double>(rows - 1);
  const int c0 = static_cast<int>(fx);
  const int r0 = static_cast<int>(fy);
  const int c1 = (std::min)(c0 + 1, cols - 1);
  const int r1 = (std::min)(r0 + 1, rows - 1);
  const float tx = static_cast<float>(fx - c0);
  const float ty = static_cast<float>(fy - r0);
  const auto at = [&](int c, int r) {
    return heights[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) +
                   static_cast<std::size_t>(c)];
  };
  const float a = at(c0, r0) * (1.f - tx) + at(c1, r0) * tx;
  const float b = at(c0, r1) * (1.f - tx) + at(c1, r1) * tx;
  return a * (1.f - ty) + b * ty;
}

void sample_rgba_bilinear(const std::vector<uint8_t>& src, int w, int h,
                          double u, double v, uint8_t out[4]) {
  if (src.empty() || w < 1 || h < 1) {
    out[0] = out[1] = out[2] = 0;
    out[3] = 0;
    return;
  }
  u = (std::max)(0.0, (std::min)(1.0, u));
  v = (std::max)(0.0, (std::min)(1.0, v));
  const double fx = u * static_cast<double>(w - 1);
  const double fy = v * static_cast<double>(h - 1);
  const int x0 = static_cast<int>(fx);
  const int y0 = static_cast<int>(fy);
  const int x1 = (std::min)(x0 + 1, w - 1);
  const int y1 = (std::min)(y0 + 1, h - 1);
  const float tx = static_cast<float>(fx - x0);
  const float ty = static_cast<float>(fy - y0);
  const auto px = [&](int x, int y) -> const uint8_t* {
    return &src[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                 static_cast<std::size_t>(x)) *
                4u];
  };
  const uint8_t* p00 = px(x0, y0);
  const uint8_t* p10 = px(x1, y0);
  const uint8_t* p01 = px(x0, y1);
  const uint8_t* p11 = px(x1, y1);
  for (int k = 0; k < 4; ++k) {
    const float a = static_cast<float>(p00[k]) * (1.f - tx) +
                    static_cast<float>(p10[k]) * tx;
    const float b = static_cast<float>(p01[k]) * (1.f - tx) +
                    static_cast<float>(p11[k]) * tx;
    out[k] = static_cast<uint8_t>((std::min)(255.f, a * (1.f - ty) + b * ty + 0.5f));
  }
}

}  // namespace

float GlobePass::surface_radius(double lon_deg, double lat_deg) const {
  const float h = sample_height(lon_deg, lat_deg);
  return params_.radius + (std::max)(0.f, h) * params_.height_scale;
}

float GlobePass::sample_detail_height(double lon, double lat) const {
  if (detail_heights_.empty() || detail_cols_ < 2 || detail_rows_ < 2) {
    return 0.f;
  }
  const float fade = dem_edge_fade(lon, lat, detail_min_lon_, detail_min_lat_,
                                   detail_max_lon_, detail_max_lat_);
  if (fade <= 1e-4f) {
    return 0.f;
  }
  return sample_grid_height(detail_heights_, detail_cols_, detail_rows_,
                            detail_min_lon_, detail_min_lat_, detail_max_lon_,
                            detail_max_lat_, lon, lat) *
         fade;
}

float GlobePass::sample_height(double lon, double lat) const {
  if (dem_heights_.empty() || dem_cols_ < 2 || dem_rows_ < 2) {
    return 0.f;
  }
  if (lon < dem_min_lon_ || lon > dem_max_lon_ || lat < dem_min_lat_ ||
      lat > dem_max_lat_) {
    return 0.f;
  }
  const float fade = dem_is_global_
                         ? 1.f
                         : dem_edge_fade(lon, lat, dem_min_lon_, dem_min_lat_,
                                         dem_max_lon_, dem_max_lat_);
  if (fade <= 1e-4f) {
    return 0.f;
  }
  const float base =
      sample_grid_height(dem_heights_, dem_cols_, dem_rows_, dem_min_lon_,
                         dem_min_lat_, dem_max_lon_, dem_max_lat_, lon, lat) *
      fade;
  if (detail_blend_ <= 1e-4f || detail_heights_.empty()) {
    return base;
  }
  const float overlay = sample_detail_height(lon, lat);
  const float overlay_fade =
      dem_edge_fade(lon, lat, detail_min_lon_, detail_min_lat_,
                    detail_max_lon_, detail_max_lat_);
  const float w = detail_blend_ * overlay_fade;
  if (w <= 1e-4f) {
    return base;
  }
  return base * (1.f - w) + overlay * w;
}

void GlobePass::sample_normal(double lon_deg, double lat_deg, float* nx,
                              float* ny, float* nz) const {
  // Finite-difference DEM slope → outward normal for vertex Lambert.
  const float hs = params_.height_scale;
  const float R = params_.radius;
  constexpr double kRad = 3.14159265358979323846 / 180.0;
  // ~one mesh cell in degrees (matches denser near-earth tessellation).
  const int nlon = (std::max)(8, params_.lon_slices);
  const int nlat = (std::max)(4, params_.lat_slices);
  const double dlon = 360.0 / static_cast<double>(nlon);
  const double dlat = 180.0 / static_cast<double>(nlat);
  const float h0 = sample_height(lon_deg, lat_deg);
  const float he = sample_height(lon_deg + dlon, lat_deg);
  const float hn = sample_height(lon_deg, lat_deg + dlat);
  const float lon0 = static_cast<float>(lon_deg * kRad);
  const float lat0 = static_cast<float>(lat_deg * kRad);
  const float lon1 = static_cast<float>((lon_deg + dlon) * kRad);
  const float lat1 = static_cast<float>((lat_deg + dlat) * kRad);
  float x0 = 0.f, y0 = 0.f, z0 = 0.f;
  float xe = 0.f, ye = 0.f, ze = 0.f;
  float xn = 0.f, yn = 0.f, zn = 0.f;
  lonlat_to_xyz(lon0, lat0, R + (std::max)(0.f, h0) * hs, &x0, &y0, &z0);
  lonlat_to_xyz(lon1, lat0, R + (std::max)(0.f, he) * hs, &xe, &ye, &ze);
  lonlat_to_xyz(lon0, lat1, R + (std::max)(0.f, hn) * hs, &xn, &yn, &zn);
  const float ex = xe - x0;
  const float ey = ye - y0;
  const float ez = ze - z0;
  const float nx_e = xn - x0;
  const float ny_e = yn - y0;
  const float nz_e = zn - z0;
  float cx = ey * nz_e - ez * ny_e;
  float cy = ez * nx_e - ex * nz_e;
  float cz = ex * ny_e - ey * nx_e;
  float len = std::sqrt(cx * cx + cy * cy + cz * cz);
  if (len < 1e-8f) {
    // Flat / degenerate — fall back to geocentric.
    len = std::sqrt(x0 * x0 + y0 * y0 + z0 * z0);
    if (len < 1e-8f) {
      *nx = 0.f;
      *ny = 1.f;
      *nz = 0.f;
      return;
    }
    *nx = x0 / len;
    *ny = y0 / len;
    *nz = z0 / len;
    return;
  }
  cx /= len;
  cy /= len;
  cz /= len;
  if (cx * x0 + cy * y0 + cz * z0 < 0.f) {
    cx = -cx;
    cy = -cy;
    cz = -cz;
  }
  *nx = cx;
  *ny = cy;
  *nz = cz;
}

void GlobePass::rebuild_equirect_albedo() {
  // Keep source atlas resolution when present (cap 2048×1024). Avoid the old
  // 1024×512 nearest-neighbor downsample that made coasts look pixelated.
  if (dem_is_global_ && dem_tex_w_ >= 64 && dem_tex_h_ >= 32) {
    equirect_w_ = (std::max)(1024, (std::min)(2048, dem_tex_w_));
    equirect_h_ = (std::max)(512, (std::min)(1024, dem_tex_h_));
  } else {
    equirect_w_ = dem_is_global_ ? 2048 : 1024;
    equirect_h_ = dem_is_global_ ? 1024 : 512;
  }

  auto classify_land_alpha = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    const int ir = static_cast<int>(r);
    const int ig = static_cast<int>(g);
    const int ib = static_cast<int>(b);
    const bool oceanish =
        (ib > ir + 18 && ib > ig + 12 && ig < 90 && ir < 80);
    return (a > 0 && !oceanish) ? static_cast<uint8_t>(255) : static_cast<uint8_t>(0);
  };

  bool filled = false;
  if (dem_is_global_ && !dem_albedo_.empty() && dem_tex_w_ >= 64 &&
      dem_tex_h_ >= 32) {
    const double aspect =
        static_cast<double>(dem_tex_w_) / static_cast<double>(dem_tex_h_);
    if (aspect > 1.6 && aspect < 2.5) {
      equirect_.assign(static_cast<std::size_t>(equirect_w_) *
                           static_cast<std::size_t>(equirect_h_) * 4u,
                       0);
      for (int y = 0; y < equirect_h_; ++y) {
        const double v = (static_cast<double>(y) + 0.5) /
                         static_cast<double>(equirect_h_);
        for (int x = 0; x < equirect_w_; ++x) {
          const double u = (static_cast<double>(x) + 0.5) /
                           static_cast<double>(equirect_w_);
          uint8_t s[4] = {};
          sample_rgba_bilinear(dem_albedo_, dem_tex_w_, dem_tex_h_, u, v, s);
          const std::size_t di =
              (static_cast<std::size_t>(y) *
                   static_cast<std::size_t>(equirect_w_) +
               static_cast<std::size_t>(x)) *
              4u;
          equirect_[di + 0] = s[0];
          equirect_[di + 1] = s[1];
          equirect_[di + 2] = s[2];
          equirect_[di + 3] = classify_land_alpha(s[0], s[1], s[2], s[3]);
        }
      }
      filled = true;
    }
  }
  if (!filled) {
    equirect_.assign(static_cast<std::size_t>(equirect_w_) *
                         static_cast<std::size_t>(equirect_h_) * 4u,
                     0);
    for (int y = 0; y < equirect_h_; ++y) {
      for (int x = 0; x < equirect_w_; ++x) {
        const std::size_t i =
            (static_cast<std::size_t>(y) *
                 static_cast<std::size_t>(equirect_w_) +
             static_cast<std::size_t>(x)) *
            4u;
        equirect_[i + 0] = 12;
        equirect_[i + 1] = 36;
        equirect_[i + 2] = 82;
        equirect_[i + 3] = 0;
      }
    }
    if (dem_cols_ >= 2 && dem_rows_ >= 2) {
      const double lon0 = dem_min_lon_;
      const double lon1 = dem_max_lon_;
      const double lat0 = dem_min_lat_;
      const double lat1 = dem_max_lat_;
      const int x0 = static_cast<int>(
          (std::max)(0.0, (lon0 + 180.0) / 360.0 * equirect_w_));
      const int x1 = static_cast<int>(
          (std::min)(static_cast<double>(equirect_w_),
                     (lon1 + 180.0) / 360.0 * equirect_w_));
      const int y0 = static_cast<int>(
          (std::max)(0.0, (90.0 - lat1) / 180.0 * equirect_h_));
      const int y1 = static_cast<int>(
          (std::min)(static_cast<double>(equirect_h_),
                     (90.0 - lat0) / 180.0 * equirect_h_));
      for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
          const double lon =
              -180.0 + (static_cast<double>(x) + 0.5) / equirect_w_ * 360.0;
          const double lat =
              90.0 - (static_cast<double>(y) + 0.5) / equirect_h_ * 180.0;
          const float fade =
              dem_is_global_
                  ? 1.f
                  : dem_edge_fade(lon, lat, lon0, lat0, lon1, lat1);
          if (fade <= 0.f) {
            continue;
          }
          const std::size_t i =
              (static_cast<std::size_t>(y) *
                   static_cast<std::size_t>(equirect_w_) +
               static_cast<std::size_t>(x)) *
              4u;
          uint8_t r = 40;
          uint8_t g = 120;
          uint8_t b = 50;
          uint8_t a = 255;
          if (!dem_albedo_.empty() && dem_tex_w_ > 0 && dem_tex_h_ > 0) {
            const double u = (lon - lon0) / (lon1 - lon0);
            const double v = (lat1 - lat) / (lat1 - lat0);
            uint8_t s[4] = {};
            sample_rgba_bilinear(dem_albedo_, dem_tex_w_, dem_tex_h_, u, v, s);
            r = s[0];
            g = s[1];
            b = s[2];
            a = s[3];
          } else {
            const float h = sample_grid_height(
                dem_heights_, dem_cols_, dem_rows_, lon0, lat0, lon1, lat1,
                lon, lat);
            if (h < 1.f) {
              r = 12;
              g = 36;
              b = 82;
              a = 0;
            } else if (h < 500.f) {
              r = 62;
              g = 108;
              b = 58;
            } else if (h < 2000.f) {
              r = 120;
              g = 118;
              b = 78;
            } else {
              // Avoid near-white peaks (Lambert saturates to chrome).
              r = 155;
              g = 145;
              b = 128;
            }
          }
          const float ocean_r = 12.f;
          const float ocean_g = 36.f;
          const float ocean_b = 82.f;
          const float fr = static_cast<float>(r) * fade + ocean_r * (1.f - fade);
          const float fg = static_cast<float>(g) * fade + ocean_g * (1.f - fade);
          const float fb = static_cast<float>(b) * fade + ocean_b * (1.f - fade);
          const float fa = static_cast<float>(a) * fade;
          equirect_[i + 0] = static_cast<uint8_t>((std::min)(255.f, fr + 0.5f));
          equirect_[i + 1] = static_cast<uint8_t>((std::min)(255.f, fg + 0.5f));
          equirect_[i + 2] = static_cast<uint8_t>((std::min)(255.f, fb + 0.5f));
          equirect_[i + 3] = static_cast<uint8_t>((std::min)(255.f, fa + 0.5f));
        }
      }
    }
  }

  // Near-earth China overlay: paste hi-res albedo into the China window.
  if (detail_blend_ > 1e-4f && detail_cols_ >= 2 && detail_rows_ >= 2) {
    const double lon0 = detail_min_lon_;
    const double lon1 = detail_max_lon_;
    const double lat0 = detail_min_lat_;
    const double lat1 = detail_max_lat_;
    const int x0 = static_cast<int>(
        (std::max)(0.0, (lon0 + 180.0) / 360.0 * equirect_w_));
    const int x1 = static_cast<int>(
        (std::min)(static_cast<double>(equirect_w_),
                   (lon1 + 180.0) / 360.0 * equirect_w_));
    const int y0 = static_cast<int>(
        (std::max)(0.0, (90.0 - lat1) / 180.0 * equirect_h_));
    const int y1 = static_cast<int>(
        (std::min)(static_cast<double>(equirect_h_),
                   (90.0 - lat0) / 180.0 * equirect_h_));
    for (int y = y0; y < y1; ++y) {
      for (int x = x0; x < x1; ++x) {
        const double lon =
            -180.0 + (static_cast<double>(x) + 0.5) / equirect_w_ * 360.0;
        const double lat =
            90.0 - (static_cast<double>(y) + 0.5) / equirect_h_ * 180.0;
        const float fade =
            dem_edge_fade(lon, lat, lon0, lat0, lon1, lat1) * detail_blend_;
        if (fade <= 1e-4f) {
          continue;
        }
        const std::size_t i =
            (static_cast<std::size_t>(y) *
                 static_cast<std::size_t>(equirect_w_) +
             static_cast<std::size_t>(x)) *
            4u;
        uint8_t r = 40;
        uint8_t g = 120;
        uint8_t b = 50;
        uint8_t a = 255;
        if (!detail_albedo_.empty() && detail_tex_w_ > 0 && detail_tex_h_ > 0) {
          const double u = (lon - lon0) / (lon1 - lon0);
          const double v = (lat1 - lat) / (lat1 - lat0);
          uint8_t s[4] = {};
          sample_rgba_bilinear(detail_albedo_, detail_tex_w_, detail_tex_h_, u,
                               v, s);
          r = s[0];
          g = s[1];
          b = s[2];
          a = classify_land_alpha(s[0], s[1], s[2], s[3]);
        } else {
          const float h = sample_grid_height(
              detail_heights_, detail_cols_, detail_rows_, lon0, lat0, lon1,
              lat1, lon, lat);
          if (h < 1.f) {
            r = 12;
            g = 36;
            b = 82;
            a = 0;
          } else if (h < 500.f) {
            r = 62;
            g = 108;
            b = 58;
          } else if (h < 2000.f) {
            r = 120;
            g = 118;
            b = 78;
          } else {
            // Avoid near-white peaks (Lambert saturates to chrome).
            r = 155;
            g = 145;
            b = 128;
          }
        }
        const float wr = 1.f - fade;
        equirect_[i + 0] = static_cast<uint8_t>(
            (std::min)(255.f, static_cast<float>(equirect_[i + 0]) * wr +
                                  static_cast<float>(r) * fade + 0.5f));
        equirect_[i + 1] = static_cast<uint8_t>(
            (std::min)(255.f, static_cast<float>(equirect_[i + 1]) * wr +
                                  static_cast<float>(g) * fade + 0.5f));
        equirect_[i + 2] = static_cast<uint8_t>(
            (std::min)(255.f, static_cast<float>(equirect_[i + 2]) * wr +
                                  static_cast<float>(b) * fade + 0.5f));
        equirect_[i + 3] = static_cast<uint8_t>(
            (std::min)(255.f, static_cast<float>(equirect_[i + 3]) * wr +
                                  static_cast<float>(a) * fade + 0.5f));
      }
    }
  }
  stamp_elevation_overlay();
  apply_dem_hillshade();
  albedo_dirty_ = false;
}

void GlobePass::stamp_elevation_overlay() {
  if (elev_overlay_.empty() || elev_overlay_w_ < 2 || elev_overlay_h_ < 2 ||
      equirect_.empty() || equirect_w_ <= 0 || equirect_h_ <= 0) {
    return;
  }
  if (elev_max_lon_ <= elev_min_lon_ || elev_max_lat_ <= elev_min_lat_) {
    return;
  }
  float mix = 1.f;
  if (!detail_heights_.empty()) {
    mix = detail_blend_;
  }
  if (mix <= 1e-4f) {
    return;
  }
  const double lon0 = elev_min_lon_;
  const double lon1 = elev_max_lon_;
  const double lat0 = elev_min_lat_;
  const double lat1 = elev_max_lat_;
  const int x0 = static_cast<int>(
      (std::max)(0.0, (lon0 + 180.0) / 360.0 * equirect_w_));
  const int x1 = static_cast<int>(
      (std::min)(static_cast<double>(equirect_w_),
                 (lon1 + 180.0) / 360.0 * equirect_w_));
  const int y0 = static_cast<int>(
      (std::max)(0.0, (90.0 - lat1) / 180.0 * equirect_h_));
  const int y1 = static_cast<int>(
      (std::min)(static_cast<double>(equirect_h_),
                 (90.0 - lat0) / 180.0 * equirect_h_));
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const double lon =
          -180.0 + (static_cast<double>(x) + 0.5) / equirect_w_ * 360.0;
      const double lat =
          90.0 - (static_cast<double>(y) + 0.5) / equirect_h_ * 180.0;
      const float fade = dem_edge_fade(lon, lat, lon0, lat0, lon1, lat1) * mix;
      if (fade <= 1e-4f) {
        continue;
      }
      const double u = (lon - lon0) / (lon1 - lon0);
      const double v = (lat1 - lat) / (lat1 - lat0);
      uint8_t s[4] = {};
      sample_rgba_bilinear(elev_overlay_, elev_overlay_w_, elev_overlay_h_, u,
                           v, s);
      if (s[3] == 0) {
        continue;
      }
      const std::size_t i =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(equirect_w_) +
           static_cast<std::size_t>(x)) *
          4u;
      const float oa = (static_cast<float>(s[3]) / 255.f) * fade;
      const float wr = 1.f - oa;
      equirect_[i + 0] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(equirect_[i + 0]) * wr +
                                static_cast<float>(s[0]) * oa + 0.5f));
      equirect_[i + 1] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(equirect_[i + 1]) * wr +
                                static_cast<float>(s[1]) * oa + 0.5f));
      equirect_[i + 2] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(equirect_[i + 2]) * wr +
                                static_cast<float>(s[2]) * oa + 0.5f));
      equirect_[i + 3] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(equirect_[i + 3]) * wr +
                                static_cast<float>(s[3]) * oa + 0.5f));
    }
  }
}

void GlobePass::apply_dem_hillshade() {
  // No-op: VertexLayout::kPositionNormalUv carries DEM surface normals and
  // the PS does Lambert. Baking sun shade into albedo would double-lit.
  (void)equirect_;
  (void)equirect_w_;
  (void)equirect_h_;
}

void GlobePass::rebuild_mesh() {
  // Clamp — corrupted params_ (DLL layout skew) used to request multi-GB
  // allocations. Near-earth skim uses up to 1024×512 for china DEM.
  constexpr int kMaxLon = 1024;
  constexpr int kMaxLat = 512;
  const int nlon =
      (std::max)(8, (std::min)(kMaxLon, params_.lon_slices));
  const int nlat =
      (std::max)(4, (std::min)(kMaxLat, params_.lat_slices));
  const int blend_q =
      (std::max)(0, (std::min)(10, static_cast<int>(std::lround(
                                       detail_blend_ * 10.f))));
  {
    std::lock_guard<std::mutex> lock(g_mesh_lod_mu);
    const auto it = g_mesh_lod.find(this);
    if (it != g_mesh_lod.end()) {
      for (const MeshLodCache& slot : it->second) {
        if (slot.nlon == nlon && slot.nlat == nlat && slot.blend_q == blend_q &&
            std::fabs(slot.radius - params_.radius) < 1e-4f &&
            std::fabs(slot.height_scale - params_.height_scale) < 1e-8f) {
          positions_ = slot.positions;
          indices_ = slot.indices;
          index_count_ = static_cast<uint32_t>(indices_.size());
          mesh_dirty_ = false;
          return;
        }
      }
    }
  }
  const std::size_t verts =
      static_cast<std::size_t>(nlon + 1) * static_cast<std::size_t>(nlat + 1);
  const std::size_t tris =
      static_cast<std::size_t>(nlon) * static_cast<std::size_t>(nlat) * 2u;
  std::vector<float> positions;
  std::vector<uint32_t> indices;
  // xyz + normal + uv (kPositionNormalUv = 8 floats).
  positions.resize(verts * 8u);
  indices.resize(tris * 3u);
  std::size_t wi = 0;
  for (int iy = 0; iy <= nlat; ++iy) {
    const float v = static_cast<float>(iy) / static_cast<float>(nlat);
    const float lat = kPi * (0.5f - v);  // +90 → -90
    for (int ix = 0; ix <= nlon; ++ix) {
      const float u = static_cast<float>(ix) / static_cast<float>(nlon);
      const float lon = kTwoPi * u - kPi;  // -180 → +180
      const double lon_deg = lon * (180.0 / kPi);
      const double lat_deg = lat * (180.0 / kPi);
      const float h = sample_height(lon_deg, lat_deg);
      const float r =
          params_.radius +
          (std::max)(0.f, h) * params_.height_scale;
      float x = 0.f;
      float y = 0.f;
      float z = 0.f;
      lonlat_to_xyz(lon, lat, r, &x, &y, &z);
      float nx = 0.f;
      float ny = 0.f;
      float nz = 0.f;
      sample_normal(lon_deg, lat_deg, &nx, &ny, &nz);
      positions[wi++] = x;
      positions[wi++] = y;
      positions[wi++] = z;
      positions[wi++] = nx;
      positions[wi++] = ny;
      positions[wi++] = nz;
      positions[wi++] = u;
      positions[wi++] = v;
    }
  }
  std::size_t ii = 0;
  for (int iy = 0; iy < nlat; ++iy) {
    for (int ix = 0; ix < nlon; ++ix) {
      const uint32_t i0 =
          static_cast<uint32_t>(iy * (nlon + 1) + ix);
      const uint32_t i1 = i0 + 1;
      const uint32_t i2 = i0 + static_cast<uint32_t>(nlon + 1);
      const uint32_t i3 = i2 + 1;
      indices[ii++] = i0;
      indices[ii++] = i2;
      indices[ii++] = i1;
      indices[ii++] = i1;
      indices[ii++] = i2;
      indices[ii++] = i3;
    }
  }
  positions_.swap(positions);
  indices_.swap(indices);
  index_count_ = static_cast<uint32_t>(indices_.size());
  MeshLodCache slot;
  slot.nlon = nlon;
  slot.nlat = nlat;
  slot.blend_q = blend_q;
  slot.radius = params_.radius;
  slot.height_scale = params_.height_scale;
  slot.positions = positions_;
  slot.indices = indices_;
  {
    std::lock_guard<std::mutex> lock(g_mesh_lod_mu);
    auto& slots = g_mesh_lod[this];
    if (slots.size() >= 6) {
      slots.erase(slots.begin());
    }
    slots.push_back(std::move(slot));
  }
  mesh_dirty_ = false;
}

void GlobePass::prewarm_mesh_lods() {
  if (!surface_ready_) {
    return;
  }
  const GlobeDrawParams saved = params_;
  const float saved_blend = detail_blend_;
  struct Tier {
    int lon;
    int lat;
    float blend;
  };
  Tier tiers[3] = {{384, 192, 0.f}, {640, 320, 0.f}, {512, 256, 0.f}};
  if (!detail_heights_.empty()) {
    tiers[1] = {640, 320, 0.5f};
    tiers[2] = {1024, 512, 1.f};
  }
  for (const Tier& t : tiers) {
    params_.lon_slices = t.lon;
    params_.lat_slices = t.lat;
    detail_blend_ = t.blend;
    mesh_dirty_ = true;
    rebuild_mesh();
  }
  params_ = saved;
  detail_blend_ = saved_blend;
  mesh_dirty_ = true;
}

void GlobePass::destroy_pipeline() {
  if (pipeline_ && pipeline_device_) {
    pipeline_device_->destroy_pipeline(pipeline_);
  }
  pipeline_ = nullptr;
  pipeline_device_ = nullptr;
}

bool GlobePass::ensure_pipeline(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (pipeline_ && pipeline_device_ == device) {
    return true;
  }
  destroy_pipeline();
  pipeline_device_ = device;
  pipeline_ = device->create_graphics_pipeline(globe_graphics_desc());
  return pipeline_ != nullptr;
}

bool GlobePass::ensure_gpu(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  // Only push mesh/albedo when dirty or the GPU handle set changes. Re-uploading
  // global_terrain every present was a multi-MB CopyDest barrier storm that
  // device-removed FlyCube and left HWND captures as solid sky clear.
  bool mesh_needs_upload = false;
  if (mesh_dirty_ || positions_.empty() || indices_.empty()) {
    rebuild_mesh();
    mesh_needs_upload = true;
  }
  bool albedo_needs_upload = false;
  if (albedo_dirty_ || equirect_.empty()) {
    rebuild_equirect_albedo();
    albedo_needs_upload = true;
  }
  if (index_count_ == 0 || equirect_w_ <= 0 || equirect_h_ <= 0) {
    return false;
  }
  if (device_ != device) {
    // Abandon prior handles (FlyCube may already be shut down).
    vertex_buffer_ = nullptr;
    index_buffer_ = nullptr;
    albedo_tex_ = nullptr;
    gpu_vb_bytes_ = 0;
    gpu_equirect_w_ = 0;
    gpu_equirect_h_ = 0;
    device_ = device;
    mesh_needs_upload = true;
    albedo_needs_upload = true;
  }
  const uint32_t vb_bytes =
      static_cast<uint32_t>(positions_.size() * sizeof(float));
  const uint32_t ib_bytes =
      static_cast<uint32_t>(indices_.size() * sizeof(uint32_t));
  if (!vertex_buffer_ || !index_buffer_ || gpu_vb_bytes_ != vb_bytes) {
    if (!detail::upload_static_mesh(&device_, &vertex_buffer_, &index_buffer_,
                                    &index_count_, device, positions_.data(),
                                    vb_bytes, indices_.data(), ib_bytes)) {
      gpu_vb_bytes_ = 0;
      return false;
    }
    gpu_vb_bytes_ = vb_bytes;
  } else if (mesh_needs_upload) {
    if (!device->upload(vertex_buffer_, positions_.data(), vb_bytes) ||
        !device->upload(index_buffer_, indices_.data(), ib_bytes)) {
      return false;
    }
  }
  if (albedo_tex_ &&
      (gpu_equirect_w_ != equirect_w_ || gpu_equirect_h_ != equirect_h_)) {
    albedo_tex_ = nullptr;
  }
  if (!albedo_tex_) {
    render::rhi::TextureDesc desc;
    desc.width = static_cast<uint32_t>(equirect_w_);
    desc.height = static_cast<uint32_t>(equirect_h_);
    desc.format = render::rhi::TextureFormat::kRgba8;
    desc.usage = render::rhi::TextureUsage::kSampled |
                 render::rhi::TextureUsage::kCopyDest;
    albedo_tex_ = device->create_texture(desc);
    gpu_equirect_w_ = equirect_w_;
    gpu_equirect_h_ = equirect_h_;
    albedo_needs_upload = true;
  }
  if (!albedo_tex_) {
    return false;
  }
  if (albedo_needs_upload) {
    const uint32_t tex_bytes =
        static_cast<uint32_t>(equirect_w_) * static_cast<uint32_t>(equirect_h_) *
        4u;
    if (!device->upload_texture(albedo_tex_, equirect_.data(), tex_bytes)) {
      return false;
    }
  }
  return true;
}

bool GlobePass::record(render::rhi::Device* device,
                       render::rhi::CommandList* list, uint32_t width,
                       uint32_t height,
                       const render::rhi::CameraMatrices* camera) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  if (!surface_ready_) {
    // Still draw a smooth unit sphere (ocean) so globe mode is never blank.
    if (dem_heights_.empty()) {
      dem_min_lon_ = -180.0;
      dem_min_lat_ = -90.0;
      dem_max_lon_ = 180.0;
      dem_max_lat_ = 90.0;
      dem_cols_ = 2;
      dem_rows_ = 2;
      dem_heights_ = {0.f, 0.f, 0.f, 0.f};
      surface_ready_ = true;
      mesh_dirty_ = true;
      albedo_dirty_ = true;
    }
  }
  if (!ensure_gpu(device) || !ensure_pipeline(device)) {
    return false;
  }

  GlobeConstants cb{};
  if (camera) {
    // Eye from view: C = -R^T * t (column-major look-at).
    const float* v = camera->view;
    const float tx = v[12];
    const float ty = v[13];
    const float tz = v[14];
    cb.eye_x = -(v[0] * tx + v[1] * ty + v[2] * tz);
    cb.eye_y = -(v[4] * tx + v[5] * ty + v[6] * tz);
    cb.eye_z = -(v[8] * tx + v[9] * ty + v[10] * tz);
  } else {
    cb.eye_x = 0.f;
    cb.eye_y = 0.f;
    cb.eye_z = 3.f;
  }
  cb.sun_x = params_.sun_x;
  cb.sun_y = params_.sun_y;
  cb.sun_z = params_.sun_z;
  cb.ambient = params_.ambient;
  cb.intensity = params_.intensity;
  cb.ocean_r = 0.05f;
  cb.ocean_g = 0.14f;
  cb.ocean_b = 0.32f;
  cb.atmos_strength = 1.15f;

  detail::set_fullscreen_viewport(list, width, height);
  detail::bind_camera_if(list, camera);
  // kDisabled + color-only pass → depth_off PSO (no DS format). Matches sky.
  detail::apply_raster(list, {pipeline_, render::rhi::BlendMode::kOpaque,
                              render::rhi::DepthMode::kDisabled});
  list->set_constants(kGlobeConstantSlot, &cb,
                      static_cast<uint32_t>(sizeof(cb)));
  list->bind_texture(albedo_tex_, 0);
  detail::draw_indexed_mesh(list, vertex_buffer_, index_buffer_,
                            8 * sizeof(float), index_count_);
  if (camera) {
    const float* v = camera->view;
    const float* p = camera->proj;
    std::fprintf(
        stderr,
        "atmosphere.globe.record idx=%u eye=(%.3f,%.3f,%.3f) R=%.2f "
        "view_t=(%.3f,%.3f,%.3f) view00=%.3f proj00=%.3f proj11=%.3f "
        "kind=%d\n",
        index_count_, cb.eye_x, cb.eye_y, cb.eye_z, params_.radius, v[12],
        v[13], v[14], v[0], p[0], p[5], static_cast<int>(camera->kind));
  } else {
    std::fprintf(stderr, "atmosphere.globe.record idx=%u cam=null R=%.2f\n",
                 index_count_, params_.radius);
  }
  return true;
}

void GlobePass::release() {
  vertex_buffer_ = nullptr;
  index_buffer_ = nullptr;
  albedo_tex_ = nullptr;
  device_ = nullptr;
  index_count_ = 0;
  gpu_vb_bytes_ = 0;
  gpu_equirect_w_ = 0;
  gpu_equirect_h_ = 0;
  std::lock_guard<std::mutex> lock(g_mesh_lod_mu);
  g_mesh_lod.erase(this);
}

}  // namespace vista
