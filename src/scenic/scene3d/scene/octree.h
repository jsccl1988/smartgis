// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_SCENE_OCTREE_H_
#define SCENIC_SCENE3D_SCENE_OCTREE_H_

#include "scenic/render/scenic_impl_export.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

// Flat scene-object list with AABB frustum cull. Rebuild copies pointers
// from Scene; there is no per-object hierarchical index.
class SCENIC_IMPL_EXPORT SceneOctree : public Renderable3d, public Movable3d {
 public:
  SceneOctree();
  ~SceneOctree() override;

  void set_show_node_box(bool show = true) { show_node_box_ = show; }
  bool show_node_box() const { return show_node_box_; }

  long rebuild(Object3dPtrs& objects);
  long clear();

  long Update(LP3DRENDERDEVICE device, float elapsed) override;
  long Render(LP3DRENDERDEVICE device) override;

  void debug_string(char* buf, int buf_length) const;

  void multiply_object_model_matrices(Matrix& transform);
  void multiply_object_world_matrices(Matrix& transform);

  long select_objects(Object3dPtrs& selected, LP3DRENDERDEVICE device,
                      const lPoint& point);

  const Aabb& aabb() const { return aabb_; }

 private:
  void merge_aabb();

  Object3dPtrs objects_;
  Aabb aabb_;
  bool show_node_box_ = true;
  int all_targets_ = 0;
  int cur_targets_ = 0;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_SCENE_OCTREE_H_
