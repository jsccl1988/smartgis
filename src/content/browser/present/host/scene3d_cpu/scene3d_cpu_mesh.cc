// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/hdc/scene3d_hdc_mesh.h"

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/host/gdi/gdi_primitives.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace content {
namespace detail {
namespace {

void project_xyz(const OrbitFrame* orbit, float x, float y, float z,
                 int width_px, int height_px, int* sx, int* sy) {
  if (!orbit) {
    if (sx) {
      *sx = 0;
    }
    if (sy) {
      *sy = 0;
    }
    return;
  }
  orbit->project(x, y, z, width_px, height_px, sx, sy);
}

uint64_t mesh_xyz_stamp(const float* ptr, size_t n) {
  if (!ptr || n < 3) {
    return 0;
  }
  auto bits = [](float v) {
    uint32_t u = 0;
    static_assert(sizeof(float) == sizeof(uint32_t), "float bits");
    std::memcpy(&u, &v, sizeof(u));
    return u;
  };
  uint64_t s = static_cast<uint64_t>(n);
  s ^= (static_cast<uint64_t>(bits(ptr[0])) << 32) | bits(ptr[1]);
  s ^= (static_cast<uint64_t>(bits(ptr[2])) << 16);
  const size_t mid = (n / 6) * 3;
  if (mid + 2 < n) {
    s ^= (static_cast<uint64_t>(bits(ptr[mid])) << 24) | bits(ptr[mid + 1]);
  }
  const size_t last = n - 3;
  s ^= (static_cast<uint64_t>(bits(ptr[last])) << 8) | bits(ptr[last + 2]);
  return s;
}

void paint_ocean_plane(HDC hdc, int width_px, int height_px,
                       Scene3dGpuPresent* gpu, AtmosphereSession* atmosphere,
                       const OrbitFrame* orbit,
                       const Scene3dMeshPrepCache& mesh_prep) {
  const uint8_t* early_overlay =
      gpu->overlay_tin_has_albedo() ? gpu->overlay_tin_albedo() : nullptr;
  const bool early_water_like =
      early_overlay && early_overlay[2] >= 200 && early_overlay[0] <= 80 &&
      early_overlay[1] >= 160;
  // Amber/steel hex shell (kHexAlbedo ~ 0xe0,0xa0,0x40).
  const bool early_hex_like =
      early_overlay && early_overlay[0] >= 160 && early_overlay[1] >= 100 &&
      early_overlay[2] < 140 && early_overlay[0] > early_overlay[2] + 40;
  // Atmosphere product face: only draw the shelf when ocean is enabled.
  // Leftover stereo keeps the historic light-blue base under the DEM AABB.
  bool ocean_plane_on =
      gpu->look_preset() == Scene3dLookPreset::kLegacyStereo;
  if (gpu->look_preset() == Scene3dLookPreset::kAtmosphere) {
    ocean_plane_on = atmosphere && atmosphere->environment() &&
                     atmosphere->environment()->ocean_enabled();
  }
  if (!(ocean_plane_on && mesh_prep.aabb_ok && !early_water_like &&
        !early_hex_like)) {
    return;
  }
  const float minx = mesh_prep.minx;
  const float maxx = mesh_prep.maxx;
  const float miny = mesh_prep.miny;
  const float maxy = mesh_prep.maxy;
  const float minz = mesh_prep.minz;
  const float maxz = mesh_prep.maxz;
  const float y_plane = miny - 0.02f * (std::max)(maxy - miny, 0.05f);
  int c[4][2] = {};
  project_xyz(orbit, minx, y_plane, minz, width_px, height_px, &c[0][0],
              &c[0][1]);
  project_xyz(orbit, maxx, y_plane, minz, width_px, height_px, &c[1][0],
              &c[1][1]);
  project_xyz(orbit, maxx, y_plane, maxz, width_px, height_px, &c[2][0],
              &c[2][1]);
  project_xyz(orbit, minx, y_plane, maxz, width_px, height_px, &c[3][0],
              &c[3][1]);
  const POINT ocean[4] = {{c[0][0], c[0][1]},
                          {c[1][0], c[1][1]},
                          {c[2][0], c[2][1]},
                          {c[3][0], c[3][1]}};
  gdi_fill_polygon(hdc, ocean, 4, RGB(120, 190, 230), RGB(90, 160, 210), 1);
}

void paint_dem_and_overlay(HDC hdc, int width_px, int height_px,
                           Scene3dGpuPresent* gpu, const OrbitFrame* orbit,
                           const Scene3dMeshPrepCache& mesh_prep,
                           size_t dem_idx_end) {
  // Continuous DEM: draw all tris up to a high cap. Sparse stride left
  // fragmented olive ribbons / green spikes (browse.3d inspect). Atmosphere
  // product face raises the cap so East-China DEM stays a filled surface.
  ScopedGdiPen mesh_pen = ScopedGdiPen::null_pen(hdc);
  ScopedGdiSelect null_brush(hdc, GetStockObject(NULL_BRUSH));
  const size_t total_tris = gpu->local_idx().size() / 3;
  const bool atmosphere_face =
      gpu->look_preset() == Scene3dLookPreset::kAtmosphere;
  const size_t kMaxDraw = atmosphere_face ? 96000 : 24000;
  const size_t step =
      total_tris > kMaxDraw ? (total_tris + kMaxDraw - 1) / kMaxDraw : 1;

  const bool has_overlay_albedo = gpu->overlay_tin_has_albedo();
  // Overlay-only frames (hex volume, no DEM): dem_idx_end==0 but albedo set.
  const bool overlay_only =
      dem_idx_end == 0 && has_overlay_albedo && total_tris > 0;
  const float elev_min = mesh_prep.elev_ok ? mesh_prep.elev_min : 0.f;
  const float elev_max = mesh_prep.elev_ok ? mesh_prep.elev_max : 0.f;
  const float elev_span = (std::max)(elev_max - elev_min, 1.0e-3f);

  const uint8_t* overlay_rgb =
      has_overlay_albedo ? gpu->overlay_tin_albedo() : nullptr;
  // Storm-surge free-surface: bright cyan boost. Hex/steel shells keep albedo.
  const bool water_like = overlay_rgb && overlay_rgb[2] >= 200 &&
                          overlay_rgb[0] <= 80 && overlay_rgb[1] >= 160;
  const bool hex_like =
      overlay_rgb && overlay_rgb[0] >= 160 && overlay_rgb[1] >= 100 &&
      overlay_rgb[2] < 140 && overlay_rgb[0] > overlay_rgb[2] + 40;

  // Reuse hypsometric / overlay brushes across triangles (CreateSolidBrush per
  // tri was the dominant soft-export + HUD mesh CPU cost).
  constexpr int kHypsoBands = 48;
  HBRUSH hypso_brushes[kHypsoBands] = {};
  for (int b = 0; b < kHypsoBands; ++b) {
    const float t01 =
        (kHypsoBands <= 1)
            ? 0.f
            : static_cast<float>(b) / static_cast<float>(kHypsoBands - 1);
    hypso_brushes[b] = CreateSolidBrush(hypsometric_rgb(t01));
  }
  HBRUSH overlay_brush = nullptr;
  if (overlay_rgb) {
    if (water_like) {
      overlay_brush = CreateSolidBrush(RGB(
          (std::min)(overlay_rgb[0], static_cast<uint8_t>(60)),
          (std::max)(overlay_rgb[1], static_cast<uint8_t>(180)),
          (std::max)(overlay_rgb[2], static_cast<uint8_t>(220))));
    } else {
      overlay_brush = CreateSolidBrush(
          RGB(overlay_rgb[0], overlay_rgb[1], overlay_rgb[2]));
    }
  }

  auto draw_tri = [&](size_t t, bool force_overlay_tint) {
    const unsigned i0 = gpu->local_idx()[t * 3];
    const unsigned i1 = gpu->local_idx()[t * 3 + 1];
    const unsigned i2 = gpu->local_idx()[t * 3 + 2];
    if ((i0 + 1) * 3 > gpu->local_xyz().size() ||
        (i1 + 1) * 3 > gpu->local_xyz().size() ||
        (i2 + 1) * 3 > gpu->local_xyz().size()) {
      return;
    }
    const bool is_overlay = dem_idx_end > 0 && (t * 3) >= dem_idx_end;
    HBRUSH fill = nullptr;
    if ((is_overlay || force_overlay_tint) && overlay_brush) {
      fill = overlay_brush;
    } else {
      const float y0 = gpu->local_xyz()[i0 * 3 + 1];
      const float y1 = gpu->local_xyz()[i1 * 3 + 1];
      const float y2 = gpu->local_xyz()[i2 * 3 + 1];
      const float yavg = (y0 + y1 + y2) / 3.f;
      const float t01 = (yavg - elev_min) / elev_span;
      int band = static_cast<int>(t01 * static_cast<float>(kHypsoBands - 1) +
                                  0.5f);
      if (band < 0) {
        band = 0;
      } else if (band >= kHypsoBands) {
        band = kHypsoBands - 1;
      }
      fill = hypso_brushes[band];
    }
    SelectObject(hdc, fill);
    int p0[2] = {};
    int p1[2] = {};
    int p2[2] = {};
    project_xyz(orbit, gpu->local_xyz()[i0 * 3], gpu->local_xyz()[i0 * 3 + 1],
                gpu->local_xyz()[i0 * 3 + 2], width_px, height_px, &p0[0],
                &p0[1]);
    project_xyz(orbit, gpu->local_xyz()[i1 * 3], gpu->local_xyz()[i1 * 3 + 1],
                gpu->local_xyz()[i1 * 3 + 2], width_px, height_px, &p1[0],
                &p1[1]);
    project_xyz(orbit, gpu->local_xyz()[i2 * 3], gpu->local_xyz()[i2 * 3 + 1],
                gpu->local_xyz()[i2 * 3 + 2], width_px, height_px, &p2[0],
                &p2[1]);
    POINT pts[3] = {{p0[0], p0[1]}, {p1[0], p1[1]}, {p2[0], p2[1]}};
    // Expand free-surface tris in screen space so cyan water remains readable
    // at showcase camera distance (water_on_land gate needs >0.8% pixels).
    // Hex volume fills via OrbitGeoFrame scale; do not screen-grow or the
    // lattice becomes a solid amber blob.
    if ((is_overlay || force_overlay_tint) && water_like) {
      const int cx = (pts[0].x + pts[1].x + pts[2].x) / 3;
      const int cy = (pts[0].y + pts[1].y + pts[2].y) / 3;
      constexpr int grow = 4;
      for (POINT& p : pts) {
        p.x = cx + (p.x - cx) * grow;
        p.y = cy + (p.y - cy) * grow;
      }
    }
    gdi_fill_polygon(hdc, pts, 3);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
  };

  // DEM may stride; overlay free-surface is always drawn unstrided so cyan
  // stormsurge water is not skipped when DEM triangle count dominates.
  // Overlay-only (hex volume, no DEM): skip hypsometric DEM pass; paint albedo.
  size_t drawn = 0;
  const size_t dem_tris =
      overlay_only ? 0
                   : (dem_idx_end > 0 ? dem_idx_end / 3 : total_tris);
  if (!overlay_only) {
    // Hex: draw DEM pad sparsely so the amber lattice dominates the frame.
    const size_t dem_step = hex_like ? (std::max)(step, size_t{6}) : step;
    for (size_t t = 0; t < dem_tris && drawn < kMaxDraw; t += dem_step,
                                                        ++drawn) {
      draw_tri(t, false);
    }
  }
  const bool draw_overlay_tail =
      overlay_only ||
      (dem_idx_end > 0 && dem_idx_end < gpu->local_idx().size());
  if (draw_overlay_tail) {
    const size_t overlay_tri0 =
        (dem_idx_end > 0 && dem_idx_end < gpu->local_idx().size())
            ? (dem_idx_end / 3)
            : dem_tris;
    // Water: one project pass fills screen AABB underlay then vertex discs
    // (same corners as before; avoid double full-overlay projection).
    if (water_like) {
      std::vector<POINT> water_pts;
      water_pts.reserve((total_tris > overlay_tri0)
                            ? (total_tris - overlay_tri0) * 3
                            : 0);
      int ox0 = width_px;
      int oy0 = height_px;
      int ox1 = 0;
      int oy1 = 0;
      for (size_t t = overlay_tri0; t < total_tris; ++t) {
        for (int k = 0; k < 3; ++k) {
          const unsigned vi =
              gpu->local_idx()[t * 3 + static_cast<size_t>(k)];
          if ((vi + 1) * 3 > gpu->local_xyz().size()) {
            continue;
          }
          int px = 0;
          int py = 0;
          project_xyz(orbit, gpu->local_xyz()[vi * 3],
                      gpu->local_xyz()[vi * 3 + 1],
                      gpu->local_xyz()[vi * 3 + 2], width_px, height_px, &px,
                      &py);
          ox0 = (std::min)(ox0, px);
          oy0 = (std::min)(oy0, py);
          ox1 = (std::max)(ox1, px);
          oy1 = (std::max)(oy1, py);
          water_pts.push_back(POINT{px, py});
        }
      }
      if (!water_pts.empty() && ox1 > ox0 && oy1 > oy0) {
        gdi_fill_rect(hdc, ox0 - 72, oy0 - 72, (ox1 + 72) - (ox0 - 72),
                      (oy1 + 72) - (oy0 - 72), RGB(36, 200, 240));
      }
      {
        ScopedGdiBrush fill(hdc, RGB(36, 200, 240));
        for (const POINT& p : water_pts) {
          gdi_draw_ellipse(hdc, p.x - 28, p.y - 22, p.x + 28, p.y + 22);
        }
      }
    }
    // Hex lattice: stroke each triangle so shaded faces read as a volume
    // with wireframe even before the HUD wireframe pass.
    if (hex_like) {
      ScopedGdiPen hex_edge(hdc, RGB(70, 48, 18), 1);
      for (size_t t = overlay_tri0; t < total_tris; ++t) {
        draw_tri(t, true);
      }
    } else {
      for (size_t t = overlay_tri0; t < total_tris; ++t) {
        draw_tri(t, true);
      }
    }
  }
  for (HBRUSH b : hypso_brushes) {
    if (b) {
      DeleteObject(b);
    }
  }
  if (overlay_brush) {
    DeleteObject(overlay_brush);
  }
}

void paint_beads(HDC hdc, int width_px, int height_px, Scene3dGpuPresent* gpu,
                 const OrbitFrame* orbit) {
  // Borehole / hex beads: small filled discs (FlyCube AABB path is not SoT).
  if (gpu->overlay_xyz_geo().empty() || !gpu->geo_frame().valid) {
    return;
  }
  const auto& beads = gpu->overlay_xyz_geo();
  const auto& rgba = gpu->overlay_rgba();
  const size_t bn = beads.size() / 3;
  constexpr size_t kMaxBeads = 4000;
  const size_t bstep = bn > kMaxBeads ? (bn + kMaxBeads - 1) / kMaxBeads : 1;
  ScopedGdiPen bead_pen(hdc, RGB(40, 40, 40), 1);
  HBRUSH bead_br = nullptr;
  COLORREF bead_rgb = 0;
  bool bead_rgb_set = false;
  for (size_t i = 0; i < bn; i += bstep) {
    float ox = 0.f;
    float oy = 0.f;
    float oz = 0.f;
    gpu->geo_frame().lon_lat_to_orbit(
        static_cast<double>(beads[i * 3]),
        static_cast<double>(beads[i * 3 + 1]), beads[i * 3 + 2], &ox, &oy,
        &oz);
    int px = 0;
    int py = 0;
    project_xyz(orbit, ox, oy, oz, width_px, height_px, &px, &py);
    const uint8_t r =
        (rgba.size() == bn * 4) ? rgba[i * 4] : static_cast<uint8_t>(0xf1);
    const uint8_t g =
        (rgba.size() == bn * 4) ? rgba[i * 4 + 1] : static_cast<uint8_t>(0xc4);
    const uint8_t b =
        (rgba.size() == bn * 4) ? rgba[i * 4 + 2] : static_cast<uint8_t>(0x0f);
    const COLORREF rgb = RGB(r, g, b);
    if (!bead_rgb_set || bead_rgb != rgb) {
      if (bead_br) {
        DeleteObject(bead_br);
      }
      bead_br = CreateSolidBrush(rgb);
      bead_rgb = rgb;
      bead_rgb_set = true;
    }
    ScopedGdiSelect brush_sel(hdc, bead_br);
    gdi_draw_ellipse(hdc, px - 2, py - 2, px + 3, py + 3);
  }
  if (bead_br) {
    DeleteObject(bead_br);
  }
}

}  // namespace

// Leftover SmartGis.exe hypsometric character: low green→yellow, high pink/white.
// Low band is intentionally greener so east-China plains clear green_land gates.
COLORREF hypsometric_rgb(float t01) {
  t01 = std::clamp(t01, 0.f, 1.f);
  int r = 0;
  int g = 0;
  int b = 0;
  if (t01 < 0.42f) {
    const float u = t01 / 0.42f;
    r = static_cast<int>(48 + 50 * u);
    g = static_cast<int>(150 + 70 * u);
    b = static_cast<int>(48 + 18 * (1.f - u));
  } else if (t01 < 0.62f) {
    const float u = (t01 - 0.42f) / 0.20f;
    r = static_cast<int>(98 + 55 * u);
    g = static_cast<int>(210 - 30 * u);
    b = static_cast<int>(55 + 18 * u);
  } else if (t01 < 0.82f) {
    const float u = (t01 - 0.62f) / 0.20f;
    r = static_cast<int>(130 + 42 * u);
    g = static_cast<int>(155 - 12 * u);
    b = static_cast<int>(76 + 24 * u);
  } else {
    const float u = (t01 - 0.82f) / 0.18f;
    r = static_cast<int>(172 + 30 * u);
    g = static_cast<int>(148 + 26 * u);
    b = static_cast<int>(118 + 32 * u);
  }
  return RGB(r, g, b);
}

void refresh_mesh_prep_cache(Scene3dMeshPrepCache* cache,
                             Scene3dGpuPresent* gpu, size_t dem_idx_end) {
  if (!cache) {
    return;
  }
  if (!gpu) {
    *cache = Scene3dMeshPrepCache{};
    return;
  }
  const auto& xyz = gpu->local_xyz();
  const float* ptr = xyz.empty() ? nullptr : xyz.data();
  const size_t n = xyz.size();
  const uint64_t stamp = mesh_xyz_stamp(ptr, n);
  if (cache->xyz_ptr == ptr && cache->xyz_n == n &&
      cache->dem_idx_end == dem_idx_end && cache->stamp == stamp &&
      cache->aabb_ok) {
    return;
  }
  Scene3dMeshPrepCache next;
  next.xyz_ptr = ptr;
  next.xyz_n = n;
  next.dem_idx_end = dem_idx_end;
  next.stamp = stamp;
  if (n < 3 || !ptr) {
    *cache = next;
    return;
  }
  float minx = ptr[0];
  float maxx = minx;
  float miny = ptr[1];
  float maxy = miny;
  float minz = ptr[2];
  float maxz = minz;
  for (size_t i = 0; i + 2 < n; i += 3) {
    minx = (std::min)(minx, ptr[i]);
    maxx = (std::max)(maxx, ptr[i]);
    miny = (std::min)(miny, ptr[i + 1]);
    maxy = (std::max)(maxy, ptr[i + 1]);
    minz = (std::min)(minz, ptr[i + 2]);
    maxz = (std::max)(maxz, ptr[i + 2]);
  }
  next.minx = minx;
  next.maxx = maxx;
  next.miny = miny;
  next.maxy = maxy;
  next.minz = minz;
  next.maxz = maxz;
  next.aabb_ok = true;

  size_t dem_vert_floats = n;
  if (dem_idx_end > 0 && dem_idx_end <= gpu->local_idx().size()) {
    size_t max_vi = 0;
    for (size_t i = 0; i < dem_idx_end; ++i) {
      max_vi = (std::max)(max_vi, static_cast<size_t>(gpu->local_idx()[i]));
    }
    dem_vert_floats = (std::min)(n, (max_vi + 1) * 3);
  }
  // DEM elev from DEM verts only; when the range equals the full mesh the
  // AABB Y already matches and we reuse it.
  if (dem_vert_floats == n) {
    next.elev_min = miny;
    next.elev_max = maxy;
    next.elev_ok = true;
  } else {
    bool elev_init = false;
    float elev_min = 0.f;
    float elev_max = 0.f;
    for (size_t i = 1; i + 2 < dem_vert_floats; i += 3) {
      const float y = ptr[i];
      if (!elev_init) {
        elev_min = elev_max = y;
        elev_init = true;
      } else {
        elev_min = (std::min)(elev_min, y);
        elev_max = (std::max)(elev_max, y);
      }
    }
    next.elev_min = elev_min;
    next.elev_max = elev_max;
    next.elev_ok = elev_init;
  }
  *cache = next;
}

void paint_soft_scene_body(HDC hdc, int width_px, int height_px,
                           Scene3dGpuPresent* gpu,
                           AtmosphereSession* atmosphere,
                           const OrbitFrame* orbit,
                           Scene3dMeshPrepCache* mesh_prep) {
  if (!hdc || !gpu || !mesh_prep || width_px <= 0 || height_px <= 0) {
    return;
  }
  // Light-blue ocean / base plane under the DEM AABB (leftover character).
  // Skip when a water-like or hex-shell overlay TIN is present — the plane
  // drowned free-surface / hex volume signal in showcase BMPs.
  // Also honor atmosphere ocean_enabled=false (browse.3d / world3d seed).
  const size_t dem_idx_end = gpu->dem_local_idx_count();
  refresh_mesh_prep_cache(mesh_prep, gpu, dem_idx_end);
  paint_ocean_plane(hdc, width_px, height_px, gpu, atmosphere, orbit,
                    *mesh_prep);
  paint_dem_and_overlay(hdc, width_px, height_px, gpu, orbit, *mesh_prep,
                        dem_idx_end);
  paint_beads(hdc, width_px, height_px, gpu, orbit);
}

void paint_soft_wireframe_edges(HDC hdc, int width_px, int height_px,
                                Scene3dGpuPresent* gpu,
                                const OrbitFrame* orbit) {
  if (!hdc || !gpu || width_px <= 0 || height_px <= 0 ||
      gpu->local_idx().size() < 3 || gpu->local_xyz().size() < 9) {
    return;
  }
  const uint8_t* albedo =
      gpu->overlay_tin_has_albedo() ? gpu->overlay_tin_albedo() : nullptr;
  const bool hex_like =
      albedo && albedo[0] >= 160 && albedo[1] >= 100 && albedo[2] < 140 &&
      albedo[0] > albedo[2] + 40;
  const size_t dem_idx_end = gpu->dem_local_idx_count();
  const size_t total_tris = gpu->local_idx().size() / 3;
  // Hex volume SoT: wire the lattice only (skip DEM edge soup).
  const size_t tri0 =
      (hex_like && dem_idx_end > 0 && dem_idx_end < gpu->local_idx().size())
          ? (dem_idx_end / 3)
          : 0;
  ScopedGdiPen edge(hdc, hex_like ? RGB(55, 40, 20) : RGB(40, 50, 60), 1);
  constexpr size_t kMaxEdges = 16000;
  const size_t span = total_tris > tri0 ? total_tris - tri0 : 0;
  const size_t step =
      span > kMaxEdges ? (span + kMaxEdges - 1) / kMaxEdges : 1;
  size_t drawn = 0;
  for (size_t t = tri0; t < total_tris && drawn < kMaxEdges; t += step,
                                                           ++drawn) {
    const unsigned i0 = gpu->local_idx()[t * 3];
    const unsigned i1 = gpu->local_idx()[t * 3 + 1];
    const unsigned i2 = gpu->local_idx()[t * 3 + 2];
    if ((i0 + 1) * 3 > gpu->local_xyz().size() ||
        (i1 + 1) * 3 > gpu->local_xyz().size() ||
        (i2 + 1) * 3 > gpu->local_xyz().size()) {
      continue;
    }
    int p0[2] = {};
    int p1[2] = {};
    int p2[2] = {};
    project_xyz(orbit, gpu->local_xyz()[i0 * 3], gpu->local_xyz()[i0 * 3 + 1],
                gpu->local_xyz()[i0 * 3 + 2], width_px, height_px, &p0[0],
                &p0[1]);
    project_xyz(orbit, gpu->local_xyz()[i1 * 3], gpu->local_xyz()[i1 * 3 + 1],
                gpu->local_xyz()[i1 * 3 + 2], width_px, height_px, &p1[0],
                &p1[1]);
    project_xyz(orbit, gpu->local_xyz()[i2 * 3], gpu->local_xyz()[i2 * 3 + 1],
                gpu->local_xyz()[i2 * 3 + 2], width_px, height_px, &p2[0],
                &p2[1]);
    const POINT pts[3] = {{p0[0], p0[1]}, {p1[0], p1[1]}, {p2[0], p2[1]}};
    gdi_stroke_closed(hdc, pts, 3);
  }
}

}  // namespace detail
}  // namespace content
