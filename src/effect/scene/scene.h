// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_SCENE_SCENE_H_
#define EFFECT_SCENE_SCENE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "render/rhi/rhi.h"
#include "render/programs/programs.h"
#include "gis/vista/world/world.h"
#include "gis/present/style/style_types.h"

class OGRGeometry;
class OGRLayer;

namespace geo {
class Grid;
class Tin;
}

// GPU-resident instance list synced from gis::World.

namespace effect {
namespace scene {

// Maps ResolvedPaint layer type to RGBA. Multiplies the matching opacity into
// alpha. kBackground / kFill / unknown → fill_*; kLine → line_*; kCircle →
// circle_*; kSymbol without bytes still returns fill_* as a fallback tint.
void rgba_from_resolved_paint(
    const gis::style::ResolvedPaint& paint, float* r, float* g, float* b,
    float* a);

// One World node mirrored for GPU upload (layer / 3D geom pointers stay
// non-owning). Optional ResolvedPaint is applied after sync_from.
struct GpuInstance {
  uint64_t node_id;
  gis::NodeKind kind;
  double min_x;
  double min_y;
  double min_z;
  double max_x;
  double max_y;
  double max_z;
  const gis::SmtLayer* layer = nullptr;
  OGRLayer* ogr_layer = nullptr;
  const OGRGeometry* geom_3d = nullptr;
  std::vector<const OGRGeometry*> geoms;
  const geo::Tin* tin = nullptr;
  const geo::Grid* grid = nullptr;
  const gis::ModelAsset* model = nullptr;
  const gis::Tileset* tileset = nullptr;
  std::vector<std::string> visible_uris;
  // Copied from gis::Node on sync_from (terrain mesh upload seam).
  std::vector<float> terrain_positions;
  std::vector<uint32_t> terrain_indices;
  std::vector<float> terrain_uvs;
  std::vector<uint8_t> terrain_rgba;
  uint32_t terrain_tex_w = 0;
  uint32_t terrain_tex_h = 0;
  bool has_paint = false;
  gis::style::ResolvedPaint paint;
};

// Uploads tessellated OGRGeometry (2D and 3D instance Z) onto one list.
class GpuScene {
 public:
  GpuScene();
  ~GpuScene();
  GpuScene(const GpuScene&) = delete;
  GpuScene& operator=(const GpuScene&) = delete;

  uint64_t synced_generation() const { return synced_generation_; }
  size_t instance_count() const { return instances_.size(); }
  const GpuInstance* instance_at(size_t index) const;

  void sync_from(const gis::World& world);
  // Emits meshes and does not close the list, so leftover 3D VB/IB can append.
  // When |bound_camera| is non-null, the caller
  // already bound that RecordContext camera: this does not call bind_camera
  // and does not choose ortho versus perspective. Frustum cull uses
  // |bound_camera|. When it is null and no view camera is set, a temporary
  // legacy path binds ortho for raster and vectors, then perspective if a
  // 3D instance is present.
  bool record_draws(render::rhi::Device* device, render::rhi::CommandList* list,
                    uint32_t width, uint32_t height,
                    const render::rhi::CameraMatrices* bound_camera = nullptr);
  bool record(render::rhi::Device* device, render::rhi::CommandList* list,
              uint32_t width, uint32_t height);
  // Destroy uploaded buffers via the upload Device (Device must still be live).
  void release();
  // Drop mesh handles without Device::destroy_* (Device already gone / Null
  // stub leak policy). Prefer release() when the Device is still valid.
  void abandon();

  // Marks meshes dirty. rebuild_meshes uses this envelope to compute
  // world_units_per_pixel. Not a second projection.
  void set_view_ortho(double min_x, double min_y, double max_x, double max_y);
  void clear_view_ortho();
  bool has_view_ortho() const { return view_ortho_set_; }

  // Optional view camera for callers that do not pass a RecordContext camera
  // into record_draws. When set and no bound camera was passed, record_draws
  // binds it once and does not switch projection.
  void set_view_camera(const render::rhi::CameraMatrices& camera);
  void clear_view_camera();
  bool has_view_camera() const { return view_camera_set_; }

  // Default solid fill for untextured meshes without per-instance paint
  // (matches leftover brush cyan). Stored here and uploaded on record with
  // set_constants; this does not touch the command list.
  void set_solid_color(float r, float g, float b, float a);
  void set_solid_color_from_colorref(long colorref);

  // Attach Style paint to one synced instance. Does not parse StyleDocument;
  // callers resolve via gis::style first. Marks meshes dirty.
  bool set_instance_paint(size_t index, const gis::style::ResolvedPaint& paint);
  void clear_instance_paint(size_t index);

  // Solid clear color from a background layer paint (fill_color × fill_opacity).
  void set_background_paint(const gis::style::ResolvedPaint& paint);
  void clear_background_paint();
  bool has_background_paint() const { return background_set_; }

  // When composing after an atmosphere clear (ocean sky), set kLoad so land
  // begin_render_pass markers do not replace prior color.
  void set_color_load_op(render::rhi::ColorLoadOp op) { color_load_op_ = op; }
  render::rhi::ColorLoadOp color_load_op() const { return color_load_op_; }

  // Draw terrain / TIN triangle edges after the filled pass (GDI overlay uses
  // the same flag on Scene3dController). Default off.
  void set_wireframe(bool on) { wireframe_ = on; }
  bool wireframe() const { return wireframe_; }

  // Directional light for lit / lit-textured DEM (defaults match Light{}).
  void set_light(const render::programs::Light& light) { light_ = light; }
  const render::programs::Light& light() const { return light_; }

  // Attach the shared depth buffer (ocean → land → clouds). First pass clears;
  // later callers should set DepthLoadOp::kLoad via set_depth_load_op.
  void set_enable_depth(bool on) { enable_depth_ = on; }
  bool enable_depth() const { return enable_depth_; }
  void set_depth_load_op(render::rhi::DepthLoadOp op) { depth_load_op_ = op; }

  // GPU-uploaded triangle mesh for one World node (tessellated GIS geom).
  // Lit 3D kinds (terrain/model/tileset) use stride = 6 floats
  // (POSITION+NORMAL); 2D solid stays 3, textured 5.
  struct GpuMesh {
    gis::NodeKind kind;
    render::rhi::Buffer* vertex;
    render::rhi::Buffer* index;
    render::rhi::Texture* texture;
    uint32_t index_count;
    uint32_t stride;
    float solid_r;
    float solid_g;
    float solid_b;
    float solid_a;
    // Style scalars applied at tessellate time (line ribbon / circle diamond).
    float line_width;
    float circle_radius;
    // World-space AABB from the source GpuInstance (CPU frustum cull).
    float aabb_min_x;
    float aabb_min_y;
    float aabb_min_z;
    float aabb_max_x;
    float aabb_max_y;
    float aabb_max_z;
  };

  // Uploaded mesh inspection (null-device tests / debug).
  size_t mesh_count() const { return meshes_.size(); }
  const GpuMesh* mesh_at(size_t index) const;

  // Programs created on the Device passed to record. Recreated when that
  // Device pointer changes.
  render::rhi::Pipeline* solid_pipeline() const { return solid_pipeline_; }
  render::rhi::Pipeline* textured_pipeline() const { return textured_pipeline_; }
  render::rhi::Pipeline* lit_pipeline() const { return lit_pipeline_; }

 private:
  void clear_meshes();
  void destroy_pipelines();
  bool ensure_pipelines(render::rhi::Device* device);
  // Viewport size drives world_units_per_pixel for paint-aware line/circle
  // tessellation (pixel_width / circle_radius → world units).
  bool rebuild_meshes(render::rhi::Device* device, uint32_t width,
                      uint32_t height);
  void resolve_view_envelope(uint32_t width, uint32_t height, double* min_x,
                             double* min_y, double* max_x,
                             double* max_y) const;

  uint64_t synced_generation_;
  std::vector<GpuInstance> instances_;
  render::rhi::Device* upload_device_;
  render::rhi::Device* pipeline_device_ = nullptr;
  render::rhi::Pipeline* solid_pipeline_ = nullptr;
  render::rhi::Pipeline* textured_pipeline_ = nullptr;
  render::rhi::Pipeline* lit_pipeline_ = nullptr;
  render::rhi::Pipeline* lit_textured_pipeline_ = nullptr;
  std::vector<GpuMesh> meshes_;
  bool meshes_dirty_;
  uint32_t upload_width_;
  uint32_t upload_height_;
  bool view_ortho_set_;
  double view_min_x_;
  double view_min_y_;
  double view_max_x_;
  double view_max_y_;
  bool view_camera_set_;
  render::rhi::CameraMatrices view_camera_;
  float solid_r_;
  float solid_g_;
  float solid_b_;
  float solid_a_;
  bool background_set_;
  float background_r_;
  float background_g_;
  float background_b_;
  float background_a_;
  render::rhi::ColorLoadOp color_load_op_ = render::rhi::ColorLoadOp::kClear;
  bool enable_depth_ = false;
  render::rhi::DepthLoadOp depth_load_op_ = render::rhi::DepthLoadOp::kClear;
  bool wireframe_ = false;
  render::programs::Light light_{};
};

}  // namespace scene
}  // namespace effect

#endif  // EFFECT_SCENE_SCENE_H_
