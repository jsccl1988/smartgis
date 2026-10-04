// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_SURFACE_TERRAIN_H_
#define SCENIC_SCENE3D_PRIMITIVE_SURFACE_TERRAIN_H_

#include <array>
#include <memory>

#include "gis/geo/ops/indexed_tin.h"
#include "vista/terrain/dem/dem_height_field.h"
#include "scenic/render/scenic_impl_export.h"
#include "scenic/scene3d/primitive/surface/surface_base.h"

namespace scenic {
namespace detail {

// Terrain drawable: OGR TIN or DemHeightField coarse DEM mesh.
class SCENIC_IMPL_EXPORT Terrain : public SurfaceObject {
 public:
  Terrain();
  ~Terrain() override;

  long Init(::base::Vector3& pos, Material& material,
            const char* tex_name = "") override;
  long Update(LP3DRENDERDEVICE device, float elapsed) override;
  long Create(LP3DRENDERDEVICE device) override;
  long Render(LP3DRENDERDEVICE device) override;
  long Destroy() override;

  void set_color_type(int type) { color_type_ = type; }
  void set_x_scale(float scale) { x_scale_ = scale; }
  void set_y_scale(float scale) { y_scale_ = scale; }
  void set_z_scale(float scale) { z_scale_ = scale; }
  int color_type() const { return color_type_; }
  float x_scale() const { return x_scale_; }
  float y_scale() const { return y_scale_; }
  float z_scale() const { return z_scale_; }

  OGRTriangulatedSurface* terrain_surface() { return surface_; }
  long set_terrain_surface(OGRTriangulatedSurface* surf);
  long set_terrain_surface_directly(OGRTriangulatedSurface* surf);

  void set_height_field(const render::DemHeightField* field);
  void adopt_height_field(render::DemHeightField* field);
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

#endif  // SCENIC_SCENE3D_PRIMITIVE_SURFACE_TERRAIN_H_
