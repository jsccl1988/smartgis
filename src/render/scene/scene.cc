// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/scene/scene.h"

#include "sdb/layer/layer.h"
#include "sdb/model/model.h"
#include "sdb/scene/tessellate.h"

#include "ogrsf_frmts.h"

#include <cstddef>
#include <fstream>
#include <vector>

namespace render {
namespace scene {
namespace {

constexpr uint32_t kPositionStride = 3 * sizeof(float);

sdb::scene::LineCap line_cap_from_paint(const std::string& cap) {
  if (cap == "round") {
    return sdb::scene::LineCap::kRound;
  }
  if (cap == "square") {
    return sdb::scene::LineCap::kSquare;
  }
  return sdb::scene::LineCap::kButt;
}

sdb::scene::LineJoin line_join_from_paint(const std::string& join) {
  if (join == "round") {
    return sdb::scene::LineJoin::kRound;
  }
  if (join == "bevel") {
    return sdb::scene::LineJoin::kBevel;
  }
  return sdb::scene::LineJoin::kMiter;
}

// MapLibre line-* (pixels) → LineTessOptions. Dash lengths are converted to
// world units with the same world_units_per_pixel as stroke width.
sdb::scene::LineTessOptions line_options_from_paint(
    const sdb::style::ResolvedPaint& paint, double world_units_per_pixel) {
  sdb::scene::LineTessOptions options;
  options.pixel_width = paint.line_width;
  options.world_units_per_pixel = world_units_per_pixel;
  options.cap = line_cap_from_paint(paint.line_cap);
  options.join = line_join_from_paint(paint.line_join);
  options.dasharray.clear();
  options.dasharray.reserve(paint.line_dasharray.size());
  for (float dash_px : paint.line_dasharray) {
    options.dasharray.push_back(static_cast<double>(dash_px) *
                                world_units_per_pixel);
  }
  return options;
}

// Gap: tessellate.h has no radius-aware point API (tessellate_point_xy is
// fixed world half-extent 0.05). Approximate circle-radius (pixels) as a
// diamond (4 tris) scaled by world_units_per_pixel.
void append_circle_diamond(double x, double y, double z, double radius_world,
                           sdb::scene::TessMesh& out) {
  if (radius_world <= 0) {
    return;
  }
  const float r = static_cast<float>(radius_world);
  const uint32_t base =
      static_cast<uint32_t>(out.positions.size() / 3);
  const float fx = static_cast<float>(x);
  const float fy = static_cast<float>(y);
  const float fz = static_cast<float>(z);
  // Center + N/E/S/W.
  out.positions.push_back(fx);
  out.positions.push_back(fy);
  out.positions.push_back(fz);
  out.positions.push_back(fx);
  out.positions.push_back(fy + r);
  out.positions.push_back(fz);
  out.positions.push_back(fx + r);
  out.positions.push_back(fy);
  out.positions.push_back(fz);
  out.positions.push_back(fx);
  out.positions.push_back(fy - r);
  out.positions.push_back(fz);
  out.positions.push_back(fx - r);
  out.positions.push_back(fy);
  out.positions.push_back(fz);
  out.indices.push_back(base);
  out.indices.push_back(base + 1);
  out.indices.push_back(base + 2);
  out.indices.push_back(base);
  out.indices.push_back(base + 2);
  out.indices.push_back(base + 3);
  out.indices.push_back(base);
  out.indices.push_back(base + 3);
  out.indices.push_back(base + 4);
  out.indices.push_back(base);
  out.indices.push_back(base + 4);
  out.indices.push_back(base + 1);
}

void append_mesh(sdb::scene::TessMesh& dst, const sdb::scene::TessMesh& src) {
  if (src.indices.empty()) {
    return;
  }
  const uint32_t base = static_cast<uint32_t>(dst.positions.size() / 3);
  dst.positions.insert(dst.positions.end(), src.positions.begin(),
                       src.positions.end());
  for (uint32_t idx : src.indices) {
    dst.indices.push_back(base + idx);
  }
}

bool tessellate_geom_paint_aware(const OGRGeometry* geom,
                                 const sdb::style::ResolvedPaint* paint,
                                 double world_units_per_pixel,
                                 sdb::scene::TessMesh& out) {
  if (!geom) {
    return false;
  }
  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  if (paint && paint->type == sdb::style::LayerType::kLine &&
      (flat == wkbLineString || flat == wkbLinearRing)) {
    // tessellate_line resets its out mesh — always stage then merge.
    sdb::scene::TessMesh part;
    const sdb::scene::LineTessOptions options =
        line_options_from_paint(*paint, world_units_per_pixel);
    if (!sdb::scene::tessellate_line(geom->toLineString(), options, part)) {
      return false;
    }
    append_mesh(out, part);
    return true;
  }
  if (paint && paint->type == sdb::style::LayerType::kCircle &&
      flat == wkbPoint) {
    const auto* pt = geom->toPoint();
    const double radius_world =
        static_cast<double>(paint->circle_radius) * world_units_per_pixel;
    const size_t before = out.indices.size();
    append_circle_diamond(pt->getX(), pt->getY(), pt->getZ(), radius_world,
                          out);
    return out.indices.size() > before;
  }
  if (flat == wkbMultiPoint || flat == wkbMultiLineString ||
      flat == wkbMultiPolygon || flat == wkbGeometryCollection ||
      flat == wkbTIN) {
    const auto* col = geom->toGeometryCollection();
    if (!col) {
      return false;
    }
    bool any = false;
    const int n = col->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      if (tessellate_geom_paint_aware(col->getGeometryRef(i), paint,
                                      world_units_per_pixel, out)) {
        any = true;
      }
    }
    return any;
  }
  // Fill / unknown / non-styled geom types keep legacy tessellate_geometry.
  sdb::scene::TessMesh part;
  if (!sdb::scene::tessellate_geometry(geom, part)) {
    return false;
  }
  append_mesh(out, part);
  return true;
}

bool tessellate_vector_instance(const GpuInstance& inst,
                                double world_units_per_pixel,
                                sdb::scene::TessMesh& out) {
  out.positions.clear();
  out.indices.clear();
  out.has_image = false;
  const sdb::style::ResolvedPaint* paint =
      inst.has_paint ? &inst.paint : nullptr;
  const bool style_stroke =
      paint && (paint->type == sdb::style::LayerType::kLine ||
                paint->type == sdb::style::LayerType::kCircle);

  if (!style_stroke) {
    if (inst.ogr_layer) {
      return sdb::scene::tessellate_layer(inst.ogr_layer, out);
    }
    if (!inst.geoms.empty()) {
      return sdb::scene::tessellate_geoms(inst.geoms.data(), inst.geoms.size(),
                                          out);
    }
    return false;
  }

  if (inst.ogr_layer) {
    inst.ogr_layer->ResetReading();
    bool any = false;
    while (OGRFeature* feat = inst.ogr_layer->GetNextFeature()) {
      if (tessellate_geom_paint_aware(feat->GetGeometryRef(), paint,
                                      world_units_per_pixel, out)) {
        any = true;
      }
      OGRFeature::DestroyFeature(feat);
    }
    return any && !out.indices.empty();
  }
  if (!inst.geoms.empty()) {
    bool any = false;
    for (const OGRGeometry* g : inst.geoms) {
      if (tessellate_geom_paint_aware(g, paint, world_units_per_pixel, out)) {
        any = true;
      }
    }
    return any && !out.indices.empty();
  }
  return false;
}

void argb_to_rgba(uint32_t argb, float opacity, float* r, float* g, float* b,
                  float* a) {
  const float inv = 1.f / 255.f;
  if (r) {
    *r = static_cast<float>((argb >> 16) & 0xff) * inv;
  }
  if (g) {
    *g = static_cast<float>((argb >> 8) & 0xff) * inv;
  }
  if (b) {
    *b = static_cast<float>(argb & 0xff) * inv;
  }
  if (a) {
    const float base_a = static_cast<float>((argb >> 24) & 0xff) * inv;
    float op = opacity;
    if (op < 0.f) {
      op = 0.f;
    } else if (op > 1.f) {
      op = 1.f;
    }
    *a = base_a * op;
  }
}

void apply_paint_scalars(const sdb::style::ResolvedPaint& paint,
                         GpuScene::GpuMesh* mesh) {
  if (!mesh) {
    return;
  }
  mesh->line_width = paint.line_width;
  mesh->circle_radius = paint.circle_radius;
  rgba_from_resolved_paint(paint, &mesh->solid_r, &mesh->solid_g,
                           &mesh->solid_b, &mesh->solid_a);
}

render::rhi::TextureDesc texture_desc_from_bytes(uint32_t byte_size) {
  render::rhi::TextureDesc desc;
  desc.format = render::rhi::TextureFormat::kRgba8;
  if (byte_size >= 4 && (byte_size % 4) == 0) {
    const uint32_t pixels = byte_size / 4;
    uint32_t side = 1;
    while ((side + 1) * (side + 1) <= pixels) {
      ++side;
    }
    if (side * side == pixels) {
      desc.width = side;
      desc.height = side;
    } else {
      desc.width = pixels;
      desc.height = 1;
    }
  } else {
    desc.width = (byte_size + 3) / 4;
    if (desc.width == 0) {
      desc.width = 1;
    }
    desc.height = 1;
  }
  return desc;
}

bool layer_image_pixels(const sdb::SmtLayer* layer, const void** data,
                        uint32_t* byte_size) {
  if (!layer || !data || !byte_size) {
    return false;
  }
  if (layer->GetLayerType() == sdb::LYR_TITLE) {
    const auto* tiles = static_cast<const sdb::SmtTileLayer*>(layer);
    const int n = tiles->GetTileCount();
    for (int i = 0; i < n; ++i) {
      const base::SmtTile* tile = tiles->GetTile(i);
      if (tile && tile->pTileBuf && tile->lTileBufSize > 0) {
        *data = tile->pTileBuf;
        *byte_size = static_cast<uint32_t>(tile->lTileBufSize);
        return true;
      }
    }
    return false;
  }
  if (layer->GetLayerType() != sdb::LYR_RASTER) {
    return false;
  }
  const auto* raster = static_cast<const sdb::SmtRasterLayer*>(layer);
  char* buf = nullptr;
  long size = 0;
  long code = 0;
  base::fRect loc;
  if (raster->GetRasterNoClone(buf, size, loc, code) == SMT_ERR_NONE && buf &&
      size > 0) {
    *data = buf;
    *byte_size = static_cast<uint32_t>(size);
    return true;
  }
  return false;
}

render::rhi::Texture* upload_rgba_texture(render::rhi::Device* device,
                                          const void* pixels,
                                          uint32_t byte_size) {
  if (!device || !pixels || byte_size == 0) {
    return nullptr;
  }
  render::rhi::TextureDesc desc = texture_desc_from_bytes(byte_size);
  render::rhi::Texture* texture = device->create_texture(desc);
  if (!texture) {
    return nullptr;
  }
  const uint32_t upload_bytes =
      byte_size < texture->byte_size() ? byte_size : texture->byte_size();
  if (!device->upload_texture(texture, pixels, upload_bytes)) {
    device->destroy_texture(texture);
    return nullptr;
  }
  return texture;
}

render::rhi::Texture* upload_layer_texture(render::rhi::Device* device,
                                           const sdb::SmtLayer* layer) {
  const void* pixels = nullptr;
  uint32_t byte_size = 0;
  if (!layer_image_pixels(layer, &pixels, &byte_size)) {
    return nullptr;
  }
  return upload_rgba_texture(device, pixels, byte_size);
}

// Upload symbol icon pixels. Prefers in-memory bytes; if only a path is set,
// tries a binary file read. Encoded image formats (PNG/JPEG) are not decoded
// here — path must already hold raw RGBA8 (or the read is skipped).
render::rhi::Texture* upload_symbol_texture(
    render::rhi::Device* device, const sdb::style::SymbolEntry& symbol) {
  if (!device) {
    return nullptr;
  }
  if (!symbol.bytes.empty()) {
    return upload_rgba_texture(device, symbol.bytes.data(),
                               static_cast<uint32_t>(symbol.bytes.size()));
  }
  if (symbol.path.empty()) {
    return nullptr;
  }
  std::ifstream in(symbol.path, std::ios::binary | std::ios::ate);
  if (!in) {
    // Path present but unreadable (missing file or no permission): skip icon.
    return nullptr;
  }
  const std::streamoff end = in.tellg();
  if (end <= 0) {
    return nullptr;
  }
  in.seekg(0, std::ios::beg);
  std::vector<uint8_t> bytes(static_cast<size_t>(end));
  if (!in.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()))) {
    return nullptr;
  }
  // Only accept sizes that look like raw RGBA8; skip compressed image files.
  if (bytes.size() < 4 || (bytes.size() % 4) != 0) {
    return nullptr;
  }
  return upload_rgba_texture(device, bytes.data(),
                             static_cast<uint32_t>(bytes.size()));
}

bool upload_mesh(render::rhi::Device* device, const float* positions,
                 size_t position_count, const uint32_t* indices,
                 size_t index_count, bool with_uv, GpuScene::GpuMesh* out) {
  if (!device || !out || !positions || !indices || position_count == 0 ||
      index_count == 0 || (position_count % 3) != 0) {
    return false;
  }
  std::vector<float> interleaved;
  const float* vb_data = positions;
  uint32_t vb_bytes = static_cast<uint32_t>(position_count * sizeof(float));
  uint32_t stride = kPositionStride;
  if (with_uv) {
    const size_t verts = position_count / 3;
    float minx = positions[0];
    float miny = positions[1];
    float maxx = positions[0];
    float maxy = positions[1];
    for (size_t i = 0; i < verts; ++i) {
      const float x = positions[i * 3];
      const float y = positions[i * 3 + 1];
      if (x < minx) {
        minx = x;
      }
      if (y < miny) {
        miny = y;
      }
      if (x > maxx) {
        maxx = x;
      }
      if (y > maxy) {
        maxy = y;
      }
    }
    const float dx = (maxx - minx) > 1e-6f ? (maxx - minx) : 1.f;
    const float dy = (maxy - miny) > 1e-6f ? (maxy - miny) : 1.f;
    interleaved.resize(verts * 5);
    for (size_t i = 0; i < verts; ++i) {
      interleaved[i * 5 + 0] = positions[i * 3 + 0];
      interleaved[i * 5 + 1] = positions[i * 3 + 1];
      interleaved[i * 5 + 2] = positions[i * 3 + 2];
      interleaved[i * 5 + 3] = (positions[i * 3 + 0] - minx) / dx;
      interleaved[i * 5 + 4] = (positions[i * 3 + 1] - miny) / dy;
    }
    vb_data = interleaved.data();
    vb_bytes = static_cast<uint32_t>(interleaved.size() * sizeof(float));
    stride = 5 * sizeof(float);
  }
  const uint32_t ib_bytes = static_cast<uint32_t>(index_count * sizeof(uint32_t));
  out->vertex = device->create_buffer(vb_bytes, render::rhi::BufferUsage::kVertex);
  out->index = device->create_buffer(ib_bytes, render::rhi::BufferUsage::kIndex);
  if (!out->vertex || !out->index) {
    device->destroy_buffer(out->vertex);
    device->destroy_buffer(out->index);
    out->vertex = nullptr;
    out->index = nullptr;
    return false;
  }
  if (!device->upload(out->vertex, vb_data, vb_bytes) ||
      !device->upload(out->index, indices, ib_bytes)) {
    device->destroy_buffer(out->vertex);
    device->destroy_buffer(out->index);
    out->vertex = nullptr;
    out->index = nullptr;
    return false;
  }
  out->index_count = static_cast<uint32_t>(index_count);
  out->stride = stride;
  return true;
}

void record_kind(render::rhi::CommandList* list,
                 const render::rhi::RenderPassDesc& pass, uint32_t width,
                 uint32_t height, const std::vector<GpuScene::GpuMesh>& meshes,
                 sdb::scene::NodeKind kind) {
  bool any = false;
  for (const auto& mesh : meshes) {
    if (mesh.kind == kind && mesh.index_count > 0 && mesh.vertex &&
        mesh.index) {
      any = true;
      break;
    }
  }
  if (!any) {
    return;
  }
  list->begin_render_pass(pass);
  list->set_viewport(0, 0, static_cast<float>(width), static_cast<float>(height),
                     0, 1);
  for (const auto& mesh : meshes) {
    if (mesh.kind != kind || mesh.index_count == 0 || !mesh.vertex ||
        !mesh.index) {
      continue;
    }
    if (mesh.texture) {
      list->bind_texture(mesh.texture, 0);
    } else {
      list->set_solid_color(mesh.solid_r, mesh.solid_g, mesh.solid_b,
                            mesh.solid_a);
    }
    list->bind_vertex_buffer(mesh.vertex, 0,
                            mesh.stride ? mesh.stride : kPositionStride);
    list->bind_index_buffer(mesh.index, 0);
    list->draw_indexed(mesh.index_count, 1, 0, 0, 0);
  }
  list->end_render_pass();
}

}  // namespace

void rgba_from_resolved_paint(const sdb::style::ResolvedPaint& paint, float* r,
                              float* g, float* b, float* a) {
  using sdb::style::LayerType;
  uint32_t argb = paint.fill_color;
  float opacity = paint.fill_opacity;
  switch (paint.type) {
    case LayerType::kLine:
      argb = paint.line_color;
      opacity = paint.line_opacity;
      break;
    case LayerType::kCircle:
      argb = paint.circle_color;
      opacity = paint.circle_opacity;
      break;
    case LayerType::kBackground:
    case LayerType::kFill:
    case LayerType::kSymbol:
    case LayerType::kRaster:
    case LayerType::kUnknown:
    default:
      break;
  }
  argb_to_rgba(argb, opacity, r, g, b, a);
}

GpuScene::GpuScene()
    : synced_generation_(0),
      upload_device_(nullptr),
      meshes_dirty_(true),
      upload_width_(0),
      upload_height_(0),
      view_ortho_set_(false),
      view_min_x_(0),
      view_min_y_(0),
      view_max_x_(1),
      view_max_y_(1),
      solid_r_(0.f),
      solid_g_(1.f),
      solid_b_(1.f),
      solid_a_(1.f),
      background_set_(false),
      background_r_(0.f),
      background_g_(0.2f),
      background_b_(0.4f),
      background_a_(1.f) {}

GpuScene::~GpuScene() { clear_meshes(); }

void GpuScene::release() { clear_meshes(); }

void GpuScene::set_view_ortho(double min_x, double min_y, double max_x,
                              double max_y) {
  view_min_x_ = min_x;
  view_min_y_ = min_y;
  view_max_x_ = max_x;
  view_max_y_ = max_y;
  view_ortho_set_ = true;
  meshes_dirty_ = true;
}

void GpuScene::clear_view_ortho() {
  view_ortho_set_ = false;
  meshes_dirty_ = true;
}

void GpuScene::set_solid_color(float r, float g, float b, float a) {
  solid_r_ = r;
  solid_g_ = g;
  solid_b_ = b;
  solid_a_ = a;
  meshes_dirty_ = true;
}

void GpuScene::set_solid_color_from_colorref(long colorref) {
  const float r = static_cast<float>((colorref >> 0) & 0xff) / 255.f;
  const float g = static_cast<float>((colorref >> 8) & 0xff) / 255.f;
  const float b = static_cast<float>((colorref >> 16) & 0xff) / 255.f;
  set_solid_color(r, g, b, 1.f);
}

bool GpuScene::set_instance_paint(size_t index,
                                 const sdb::style::ResolvedPaint& paint) {
  if (index >= instances_.size()) {
    return false;
  }
  instances_[index].has_paint = true;
  instances_[index].paint = paint;
  meshes_dirty_ = true;
  return true;
}

void GpuScene::clear_instance_paint(size_t index) {
  if (index >= instances_.size()) {
    return;
  }
  instances_[index].has_paint = false;
  instances_[index].paint = sdb::style::ResolvedPaint();
  meshes_dirty_ = true;
}

void GpuScene::set_background_paint(const sdb::style::ResolvedPaint& paint) {
  // Background layers use fill_color / fill_opacity (style maps
  // background-color into fill_* on ResolvedPaint when present).
  argb_to_rgba(paint.fill_color, paint.fill_opacity, &background_r_,
               &background_g_, &background_b_, &background_a_);
  background_set_ = true;
}

void GpuScene::clear_background_paint() {
  background_set_ = false;
  background_r_ = 0.f;
  background_g_ = 0.2f;
  background_b_ = 0.4f;
  background_a_ = 1.f;
}

void GpuScene::clear_meshes() {
  if (upload_device_) {
    for (GpuMesh& mesh : meshes_) {
      upload_device_->destroy_buffer(mesh.vertex);
      upload_device_->destroy_buffer(mesh.index);
      upload_device_->destroy_texture(mesh.texture);
    }
  }
  meshes_.clear();
  upload_device_ = nullptr;
  upload_width_ = 0;
  upload_height_ = 0;
}

void GpuScene::resolve_view_envelope(uint32_t width, uint32_t height,
                                     double* min_x, double* min_y,
                                     double* max_x, double* max_y) const {
  double minx = 0;
  double miny = 0;
  double maxx = 1;
  double maxy = 1;
  bool have_box = false;
  if (view_ortho_set_) {
    minx = view_min_x_;
    miny = view_min_y_;
    maxx = view_max_x_;
    maxy = view_max_y_;
    have_box = true;
  } else {
    for (const GpuInstance& inst : instances_) {
      if (inst.kind != sdb::scene::NodeKind::kRasterLayer &&
          inst.kind != sdb::scene::NodeKind::kVectorLayer) {
        continue;
      }
      if (!have_box) {
        minx = inst.min_x;
        miny = inst.min_y;
        maxx = inst.max_x;
        maxy = inst.max_y;
        have_box = true;
      } else {
        if (inst.min_x < minx) {
          minx = inst.min_x;
        }
        if (inst.min_y < miny) {
          miny = inst.min_y;
        }
        if (inst.max_x > maxx) {
          maxx = inst.max_x;
        }
        if (inst.max_y > maxy) {
          maxy = inst.max_y;
        }
      }
    }
  }
  if (!have_box) {
    minx = 0;
    miny = 0;
    maxx = static_cast<double>(width);
    maxy = static_cast<double>(height);
  }
  if (maxx - minx < 1e-6) {
    maxx = minx + 1;
  }
  if (maxy - miny < 1e-6) {
    maxy = miny + 1;
  }
  if (min_x) {
    *min_x = minx;
  }
  if (min_y) {
    *min_y = miny;
  }
  if (max_x) {
    *max_x = maxx;
  }
  if (max_y) {
    *max_y = maxy;
  }
}

bool GpuScene::rebuild_meshes(render::rhi::Device* device, uint32_t width,
                             uint32_t height) {
  clear_meshes();
  if (!device || width == 0 || height == 0) {
    return false;
  }
  upload_device_ = device;

  double env_min_x = 0;
  double env_min_y = 0;
  double env_max_x = 1;
  double env_max_y = 1;
  resolve_view_envelope(width, height, &env_min_x, &env_min_y, &env_max_x,
                        &env_max_y);
  const double world_units_per_pixel =
      (env_max_x - env_min_x) / static_cast<double>(width);

  meshes_.reserve(instances_.size());
  for (const GpuInstance& inst : instances_) {
    GpuMesh mesh;
    mesh.kind = inst.kind;
    mesh.vertex = nullptr;
    mesh.index = nullptr;
    mesh.texture = nullptr;
    mesh.index_count = 0;
    mesh.stride = kPositionStride;
    mesh.solid_r = solid_r_;
    mesh.solid_g = solid_g_;
    mesh.solid_b = solid_b_;
    mesh.solid_a = solid_a_;
    mesh.line_width = 1.f;
    mesh.circle_radius = 5.f;
    if (inst.has_paint) {
      apply_paint_scalars(inst.paint, &mesh);
    }
    const bool want_symbol =
        inst.has_paint && inst.paint.has_symbol &&
        (!inst.paint.symbol.bytes.empty() || !inst.paint.symbol.path.empty());
    sdb::scene::TessMesh cpu;
    bool have = false;
    if (inst.kind == sdb::scene::NodeKind::kVectorLayer &&
        (inst.ogr_layer || !inst.geoms.empty())) {
      have = tessellate_vector_instance(inst, world_units_per_pixel, cpu);
    } else if (inst.kind == sdb::scene::NodeKind::kVectorLayer && inst.tin) {
      have = sdb::scene::tessellate_tin(inst.tin, cpu);
    } else if (inst.kind == sdb::scene::NodeKind::kVectorLayer && inst.grid) {
      have = sdb::scene::tessellate_grid(inst.grid, cpu);
    } else if (inst.kind == sdb::scene::NodeKind::kRasterLayer && inst.layer) {
      if (inst.layer->GetLayerType() == sdb::LYR_TITLE) {
        have = sdb::scene::tessellate_tile_layer(
            static_cast<const sdb::SmtTileLayer*>(inst.layer), cpu);
      } else {
        have = sdb::scene::tessellate_raster_layer(
            static_cast<const sdb::SmtRasterLayer*>(inst.layer), cpu);
      }
    } else if (inst.kind == sdb::scene::NodeKind::kModel && inst.model) {
      sdb::model::Mesh flat;
      if (sdb::model::flatten_meshes(*inst.model, flat)) {
        cpu.positions = flat.positions;
        cpu.indices = flat.indices;
        have = true;
      }
    } else if ((inst.kind == sdb::scene::NodeKind::kModel ||
                inst.kind == sdb::scene::NodeKind::kTerrain ||
                inst.kind == sdb::scene::NodeKind::kPointCloud) &&
               inst.geom_3d) {
      have = sdb::scene::tessellate_3d_geometry(inst.geom_3d, cpu);
    } else if (inst.kind == sdb::scene::NodeKind::kTileset) {
      // Prefer decoded tile content (glTF/GLB/b3dm via decode_content*). When
      // URIs are missing or tinygltf is unavailable, keep the AABB bridge.
      for (const std::string& uri : inst.visible_uris) {
        if (uri.empty()) {
          continue;
        }
        sdb::model::ModelAsset asset;
        if (!sdb::model::decode_content_file(uri.c_str(), asset)) {
          continue;
        }
        sdb::model::Mesh flat;
        if (!sdb::model::flatten_meshes(asset, flat) || flat.indices.empty()) {
          continue;
        }
        const uint32_t base =
            static_cast<uint32_t>(cpu.positions.size() / 3);
        cpu.positions.insert(cpu.positions.end(), flat.positions.begin(),
                             flat.positions.end());
        for (uint32_t idx : flat.indices) {
          cpu.indices.push_back(base + idx);
        }
        have = true;
      }
      if (!have) {
        have = sdb::scene::tessellate_aabb(inst.min_x, inst.min_y, inst.min_z,
                                           inst.max_x, inst.max_y, inst.max_z,
                                           cpu);
      }
    }
    if (!have) {
      continue;
    }
    if (!upload_mesh(device, cpu.positions.data(), cpu.positions.size(),
                     cpu.indices.data(), cpu.indices.size(),
                     cpu.has_image || want_symbol, &mesh)) {
      clear_meshes();
      return false;
    }
    if (cpu.has_image && inst.layer) {
      mesh.texture = upload_layer_texture(device, inst.layer);
    } else if (want_symbol) {
      mesh.texture = upload_symbol_texture(device, inst.paint.symbol);
    }
    meshes_.push_back(mesh);
  }
  upload_width_ = width;
  upload_height_ = height;
  meshes_dirty_ = false;
  return true;
}

const GpuScene::GpuMesh* GpuScene::mesh_at(size_t index) const {
  if (index >= meshes_.size()) {
    return nullptr;
  }
  return &meshes_[index];
}

const GpuInstance* GpuScene::instance_at(size_t index) const {
  if (index >= instances_.size()) {
    return nullptr;
  }
  return &instances_[index];
}

void GpuScene::sync_from(const sdb::scene::World& world) {
  if (world.generation() == synced_generation_) {
    return;
  }
  instances_.clear();
  const size_t n = world.node_count();
  instances_.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const sdb::scene::Node* node = world.node_at(i);
    if (!node) {
      continue;
    }
    GpuInstance inst;
    inst.node_id = node->id;
    inst.kind = node->kind;
    inst.min_x = node->min_x;
    inst.min_y = node->min_y;
    inst.min_z = node->min_z;
    inst.max_x = node->max_x;
    inst.max_y = node->max_y;
    inst.max_z = node->max_z;
    inst.layer = node->layer;
    inst.ogr_layer = node->ogr_layer;
    inst.geom_3d = node->geom_3d;
    inst.geoms = node->geoms;
    inst.tin = node->tin;
    inst.grid = node->grid;
    inst.model = node->model;
    inst.tileset = node->tileset;
    inst.visible_uris = node->visible_uris;
    inst.has_paint = false;
    instances_.push_back(inst);
  }
  synced_generation_ = world.generation();
  meshes_dirty_ = true;
}

bool GpuScene::record_draws(render::rhi::Device* device,
                             render::rhi::CommandList* list, uint32_t width,
                             uint32_t height) {
  if (!device || !list || width == 0 || height == 0) {
    return false;
  }
  if (meshes_dirty_ || upload_device_ != device || upload_width_ != width ||
      upload_height_ != height) {
    if (!rebuild_meshes(device, width, height)) {
      return false;
    }
  }

  render::rhi::RenderPassDesc pass;
  pass.clear_r = background_r_;
  pass.clear_g = background_g_;
  pass.clear_b = background_b_;
  pass.clear_a = background_a_;
  pass.width = width;
  pass.height = height;

  bool have_3d = false;
  for (const GpuInstance& inst : instances_) {
    if (inst.kind == sdb::scene::NodeKind::kModel ||
        inst.kind == sdb::scene::NodeKind::kTerrain ||
        inst.kind == sdb::scene::NodeKind::kPointCloud ||
        inst.kind == sdb::scene::NodeKind::kTileset) {
      have_3d = true;
    }
  }

  double minx = 0;
  double miny = 0;
  double maxx = 1;
  double maxy = 1;
  resolve_view_envelope(width, height, &minx, &miny, &maxx, &maxy);

  // One CommandList: raster/tile underlay, then 2D vectors, then 3D models.
  list->bind_camera(render::rhi::make_ortho_camera(
      static_cast<float>(minx), static_cast<float>(maxx),
      static_cast<float>(miny), static_cast<float>(maxy), -1.f, 1.f));
  record_kind(list, pass, width, height, meshes_,
              sdb::scene::NodeKind::kRasterLayer);
  record_kind(list, pass, width, height, meshes_,
              sdb::scene::NodeKind::kVectorLayer);
  if (have_3d) {
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    list->bind_camera(render::rhi::make_perspective_camera(0.785398f, aspect,
                                                           0.1f, 100.f));
  }
  record_kind(list, pass, width, height, meshes_,
              sdb::scene::NodeKind::kModel);
  record_kind(list, pass, width, height, meshes_,
              sdb::scene::NodeKind::kTileset);

  if (meshes_.empty()) {
    list->begin_render_pass(pass);
    list->set_viewport(0, 0, static_cast<float>(width),
                       static_cast<float>(height), 0, 1);
    list->end_render_pass();
  }
  return true;
}

bool GpuScene::record(render::rhi::Device* device,
                       render::rhi::CommandList* list, uint32_t width,
                       uint32_t height) {
  if (!record_draws(device, list, width, height)) {
    return false;
  }
  list->close();
  return true;
}

}  // namespace scene
}  // namespace render
