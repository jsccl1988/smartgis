// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_SURFACE_TERRAIN_H_
#define SCENIC_SCENE3D_PRIMITIVE_SURFACE_TERRAIN_H_

#include <array>
#include <memory>

#include "gis/geo/ops/indexed_tin.h"
#include "vista/world/terrain/dem/dem_height_field.h"
#include "scenic/scenic_impl_export.h"
#include "scenic/scene3d/primitive/surface/surface_base.h"

namespace scenic {
namespace detail {

// Leftover terrain drawable: OGR TIN or DemHeightField coarse DEM mesh.
// DemHeightField coarse DEM mesh (hypsometric color + normals).
class LEGACY_RENDER_EXPORT Terrain : public SurfaceObject {
 public:
  Terrain();
  ~Terrain() override;

  long Init(::base::Vector3& vPos, Material& matMaterial,
            const char* szTexName = "") override;
  long Update(LP3DRENDERDEVICE device, float elapsed) override;
  long Create(LP3DRENDERDEVICE device) override;
  long Render(LP3DRENDERDEVICE device) override;
  long Destroy() override;

  // Legacy plugin ABI (PascalCase kept for LoadLibrary-era callers).
  Vector3 GetCenter() { return center_; }
  void SetClrType(int type) { color_type_ = type; }
  void SetXScale(float scale) { x_scale_ = scale; }
  void SetYScale(float scale) { y_scale_ = scale; }
  void SetZScale(float scale) { z_scale_ = scale; }
  int GetClrType() const { return color_type_; }
  float GetXScale() const { return x_scale_; }
  float GetYScale() const { return y_scale_; }
  float GetZScale() const { return z_scale_; }

  OGRTriangulatedSurface* GetTerrainSurf() { return surface_; }
  long SetTerrainSurf(OGRTriangulatedSurface* surf);
  long SetTerrainSurfDirectly(OGRTriangulatedSurface* surf);

  void set_height_field(const render::DemHeightField* field);
  void adopt_height_field(render::DemHeightField* field);
  // Non-inline: dem_stereo_test / plugins must not bake field_ offsetof across
  // the scenic_impl DLL boundary (SurfaceObject base shifts layout).
  const render::DemHeightField* height_field() const;

 private:
  void sample_color(float height, Color* out) const;
  long create_from_surface(LP3DRENDERDEVICE device);
  long create_from_height_field(LP3DRENDERDEVICE device);
  long render_height_field(LP3DRENDERDEVICE device);
  void render_surface(LP3DRENDERDEVICE device);

  int color_type_ = 0;
  std::array<Color, 3> color_ramp_{};
  float z_scale_ = 1.f;
  float x_scale_ = 1.f;
  float y_scale_ = 1.f;
  float min_z_ = 0.f;
  float max_z_ = 0.f;

  OGRTriangulatedSurface* surface_ = nullptr;
  std::unique_ptr<render::DemHeightField> owned_field_;
  const render::DemHeightField* field_ = nullptr;
};

}  // namespace detail
}  // namespace scenic

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  // SCENIC_SCENE3D_PRIMITIVE_SURFACE_TERRAIN_H_
