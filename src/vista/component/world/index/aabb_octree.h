// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Spatial AABB index over unibn (jbehley/octree) point octree of box centers.
// Headers stay free of Octree.hpp.

#ifndef VISTA_COMPONENT_WORLD_INDEX_AABB_OCTREE_H_
#define VISTA_COMPONENT_WORLD_INDEX_AABB_OCTREE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/component/world/cull/frustum_aabb.h"
#include "vista/vista_export.h"

namespace vista {

// One indexed AABB. Index in rebuild order is the query hit id.
struct AabbBox {
  float min_x = 0.f;
  float min_y = 0.f;
  float min_z = 0.f;
  float max_x = 0.f;
  float max_y = 0.f;
  float max_z = 0.f;
};

// Scene + mesh spatial index. Radius query on AABB centers, then exact tests.
class VISTA_EXPORT AabbOctree {
 public:
  AabbOctree();
  ~AabbOctree();
  AabbOctree(AabbOctree&&) noexcept;
  AabbOctree& operator=(AabbOctree&&) noexcept;
  AabbOctree(const AabbOctree&) = delete;
  AabbOctree& operator=(const AabbOctree&) = delete;

  void clear();
  void rebuild(const AabbBox* boxes, size_t count);

  size_t size() const;
  bool empty() const { return size() == 0; }

  // Conservative candidates that may intersect |frustum|. False if the
  // frustum world AABB cannot be recovered (caller should scan linearly).
  bool query_frustum(const FrustumPlanes& frustum, const float view[16],
                     const float proj[16], std::vector<uint32_t>* hits) const;

  // AABB overlap (scene query). Always succeeds; empty index → empty hits.
  void query_aabb(float min_x, float min_y, float min_z, float max_x,
                  float max_y, float max_z, std::vector<uint32_t>* hits) const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_INDEX_AABB_OCTREE_H_
