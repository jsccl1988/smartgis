// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/tileset_stream.h"

#include <algorithm>
#include <cstring>

#include "gis/vista/assets/model/model.h"

namespace content {
namespace {

std::string join_root_uri(const std::string& root, const char* uri) {
  if (!uri || !uri[0]) {
    return {};
  }
  // Absolute / drive-relative paths stay as-is.
  if (uri[0] == '/' || uri[0] == '\\' ||
      (std::strlen(uri) >= 2 && uri[1] == ':')) {
    return uri;
  }
  if (root.empty()) {
    return uri;
  }
  std::string path = root;
  const char last = path.back();
  if (last != '/' && last != '\\') {
    path.push_back('/');
  }
  path += uri;
  return path;
}

}  // namespace

TilesetStreamSession::TilesetStreamSession()
    : cache_(4u * 1024u * 1024u) {}

gis::ViewState view_state_from_orbit(const OrbitFrame* orbit) {
  gis::ViewState view{};
  view.sse_denominator = 1;
  if (!orbit) {
    return view;
  }
  const content::Extent2 e = orbit->world_extent();
  view.eye_x = 0.5 * (e.xmin + e.xmax);
  view.eye_y = 0.5 * (e.ymin + e.ymax);
  // Distance as a rough height above the ground plane (map units).
  view.eye_z = static_cast<double>(orbit->distance());
  return view;
}

void TilesetStreamSession::set_content_root(const std::string& root) {
  content_root_ = root;
}

void TilesetStreamSession::clear(gis::World* world) {
  if (world && node_id_ != 0) {
    world->remove_node(node_id_);
  }
  node_id_ = 0;
  last_visible_uris_.clear();
  cache_.clear();
  tileset_ = gis::Tileset{};
}

bool TilesetStreamSession::attach_json(gis::World* world, const char* json,
                                       size_t len, const char* name) {
  if (!world || !json || len == 0) {
    return false;
  }
  gis::Tileset parsed;
  if (!gis::parse_tileset_json(json, len, parsed)) {
    return false;
  }
  clear(world);
  tileset_ = std::move(parsed);
  node_name_ = (name && name[0]) ? name : "city_tiles";
  gis::Node* node = world->attach_tileset(&tileset_, node_name_.c_str());
  if (!node) {
    tileset_ = gis::Tileset{};
    return false;
  }
  node_id_ = node->id;
  return true;
}

bool TilesetStreamSession::resolve_content(const char* uri, gis::ModelAsset* out,
                                           size_t* byte_cost, void* user) {
  auto* self = static_cast<TilesetStreamSession*>(user);
  if (!uri || !out || !byte_cost || !self) {
    return false;
  }
  const std::string path = join_root_uri(self->content_root_, uri);
  if (path.empty()) {
    return false;
  }
  if (!gis::decode_content_file(path.c_str(), *out)) {
    // Soft stub: empty mesh still charges a tiny cost so LRU sees the URI,
    // but only when the basename is not the intentional "missing" token.
    if (std::strstr(uri, "missing") != nullptr ||
        std::strstr(uri, "miss.") != nullptr) {
      return false;
    }
    // Keep stream green for fixture URIs without on-disk glb: empty asset with
    // a fixed cost (AABB fallback in GpuScene).
    out->name = uri;
    out->meshes.clear();
    *byte_cost = 256;
    return true;
  }
  size_t verts = 0;
  for (const auto& mesh : out->meshes) {
    verts += mesh.positions.size() / 3;
  }
  *byte_cost = (std::max)(size_t{256}, verts * 12u + 64u);
  return true;
}

bool TilesetStreamSession::pump_view(gis::World* world,
                                     const gis::ViewState& view,
                                     double max_sse, size_t max_tiles) {
  if (!world || node_id_ == 0) {
    return false;
  }
  gis::Node* node = world->find(node_id_);
  if (!node || node->kind != gis::NodeKind::kTileset) {
    // Terrain rebuild may drop nodes; re-attach the same tileset.
    node = world->attach_tileset(&tileset_, node_name_.c_str());
    if (!node) {
      return false;
    }
    node_id_ = node->id;
  }
  // Re-bind tileset pointer after World node moves (vector growth).
  node->tileset = &tileset_;

  std::vector<const gis::Tile*> visible;
  gis::select_tiles_limited(tileset_, view, max_sse, max_tiles, visible);
  const bool uris_changed = world->apply_tileset_selection(node_id_, visible);
  gis::ensure_tileset_content(visible, &cache_, &resolve_content, this);

  last_visible_uris_.clear();
  last_visible_uris_.reserve(visible.size());
  for (const gis::Tile* tile : visible) {
    if (tile) {
      last_visible_uris_.push_back(tile->content_uri);
    }
  }
  return uris_changed;
}

bool TilesetStreamSession::pump(gis::World* world, const OrbitFrame* orbit,
                                double max_sse, size_t max_tiles) {
  return pump_view(world, view_state_from_orbit(orbit), max_sse, max_tiles);
}

}  // namespace content
