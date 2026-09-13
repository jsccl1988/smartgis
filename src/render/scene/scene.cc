// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/scene/scene.h"

#include "sdb/layer/layer.h"
#include "sdb/model/model.h"
#include "sdb/scene/tessellate.h"

#include <cstddef>
#include <vector>

namespace render {
namespace scene {
namespace {

constexpr uint32_t kPositionStride = 3 * sizeof(float);

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

render::rhi::Texture* upload_layer_texture(render::rhi::Device* device,
                                           const sdb::SmtLayer* layer) {
  const void* pixels = nullptr;
  uint32_t byte_size = 0;
  if (!device || !layer_image_pixels(layer, &pixels, &byte_size)) {
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

GpuScene::GpuScene()
    : synced_generation_(0),
      upload_device_(nullptr),
      meshes_dirty_(true),
      view_ortho_set_(false),
      view_min_x_(0),
      view_min_y_(0),
      view_max_x_(1),
      view_max_y_(1),
      solid_r_(0.f),
      solid_g_(1.f),
      solid_b_(1.f),
      solid_a_(1.f) {}

GpuScene::~GpuScene() { clear_meshes(); }

void GpuScene::release() { clear_meshes(); }

void GpuScene::set_view_ortho(double min_x, double min_y, double max_x,
                              double max_y) {
  view_min_x_ = min_x;
  view_min_y_ = min_y;
  view_max_x_ = max_x;
  view_max_y_ = max_y;
  view_ortho_set_ = true;
}

void GpuScene::clear_view_ortho() { view_ortho_set_ = false; }

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
}

bool GpuScene::rebuild_meshes(render::rhi::Device* device) {
  clear_meshes();
  if (!device) {
    return false;
  }
  upload_device_ = device;
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
    sdb::scene::TessMesh cpu;
    bool have = false;
    if (inst.kind == sdb::scene::NodeKind::kVectorLayer && inst.ogr_layer) {
      have = sdb::scene::tessellate_layer(inst.ogr_layer, cpu);
    } else if (inst.kind == sdb::scene::NodeKind::kVectorLayer &&
               !inst.geoms.empty()) {
      have = sdb::scene::tessellate_geoms(inst.geoms.data(), inst.geoms.size(),
                                          cpu);
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
                     cpu.indices.data(), cpu.indices.size(), cpu.has_image,
                     &mesh)) {
      clear_meshes();
      return false;
    }
    if (cpu.has_image && inst.layer) {
      mesh.texture = upload_layer_texture(device, inst.layer);
    }
    meshes_.push_back(mesh);
  }
  meshes_dirty_ = false;
  return true;
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
  if (meshes_dirty_ || upload_device_ != device) {
    if (!rebuild_meshes(device)) {
      return false;
    }
  }

  render::rhi::RenderPassDesc pass;
  pass.clear_r = 0;
  pass.clear_g = 0.2f;
  pass.clear_b = 0.4f;
  pass.clear_a = 1;
  pass.width = width;
  pass.height = height;

  bool have_3d = false;
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
  }
  for (const GpuInstance& inst : instances_) {
    if (!view_ortho_set_ &&
        (inst.kind == sdb::scene::NodeKind::kRasterLayer ||
         inst.kind == sdb::scene::NodeKind::kVectorLayer)) {
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
    if (inst.kind == sdb::scene::NodeKind::kModel ||
        inst.kind == sdb::scene::NodeKind::kTerrain ||
        inst.kind == sdb::scene::NodeKind::kPointCloud ||
        inst.kind == sdb::scene::NodeKind::kTileset) {
      have_3d = true;
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
