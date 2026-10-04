// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SCENIC_SCENE3D_SCENE_OBJECT_H
#define SCENIC_SCENE3D_SCENE_OBJECT_H

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/base.h"

namespace scenic {
namespace detail {

class RenderDevice3d;
typedef RenderDevice3d* LP3DRENDERDEVICE;

// Base for leftover drawables that participate in scene visibility / materials.
class Renderable3d {
 public:
  Renderable3d() { m_bVisible = true; }
  virtual ~Renderable3d() {}

 public:
  bool IsVisible(void) { return m_bVisible; }
  void SetVisible(bool bVisible = true) { m_bVisible = bVisible; }

  double GetTransparent(void) { return m_dbfTransparent; }
  void SetTransparent(double dbfTransparent = 1.) {
    m_dbfTransparent = dbfTransparent;
  }

  virtual long Render(LP3DRENDERDEVICE p3DRenderDevice) = 0;
  virtual bool Select(LP3DRENDERDEVICE p3DRenderDevice, const lPoint& point) {
    return false;
  }

  inline Aabb& GetAabb() { return m_aAbb; }
  inline void SetAabb(Aabb& aabb) { m_aAbb = aabb; }

  inline Material& GetMaterial() { return m_matMaterial; }
  inline void SetMaterial(Material& matMaterial) {
    m_matMaterial = matMaterial;
  }

 protected:
  bool m_bVisible;
  double m_dbfTransparent;
  Aabb m_aAbb;
  Material m_matMaterial;
  string m_strTexName;
};

// Base for leftover objects that own a model matrix and tick Update.
class Movable3d {
 public:
  Movable3d() {}
  virtual ~Movable3d() {}

 public:
  inline Matrix& GetModelTransMatrix() { return m_mtxModel; }
  inline void SetModelTransMatrix(Matrix& vTransform) {
    m_mtxModel = vTransform;
  }
  void ModelTransMatrixMultiply(Matrix& matTransform) {
    m_mtxModel = m_mtxModel * matTransform;
  }

  virtual long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) = 0;

 protected:
  Vector3 m_vOrgPos;
  Matrix m_mtxModel;
};

// Leftover scene object: renderable + movable with a world matrix.
// Exported so out-of-line prefers_immediate_context is visible to ui_legacy.
class SCENIC_IMPL_EXPORT Object3d : public Renderable3d,
                                         public Movable3d {
 public:
  virtual long Init(::base::Vector3& vPos, Material& matMaterial,
                    const char* szTexName = "") {
    m_vOrgPos = vPos;
    m_matMaterial = matMaterial;
    m_strTexName = szTexName;
    m_mtxModel.identity();
    m_mtxWorld.identity();

    return kErrNone;
  }
  virtual long Create(LP3DRENDERDEVICE p3DRenderDevice) = 0;
  virtual long Render(LP3DRENDERDEVICE p3DRenderDevice) = 0;
  virtual long Destroy() = 0;

  // Screen-space draws (labels/sprites) must stay on the immediate D3D
  // context — deferred workers can drop or scramble CreateTexture/Draw.
  // Inline default: out-of-line in object.cc was not exported to ui_legacy
  // (LNK2001). MapLabelBatch overrides to true.
  virtual bool prefers_immediate_context() const { return false; }

  inline Matrix& GetWorldTransMatrix() { return m_mtxWorld; }
  inline void SetWorldTransMatrix(Matrix& vTransform) {
    m_mtxWorld = vTransform;
  }
  void WorldTransMatrixMultiply(Matrix& matTransform) {
    m_mtxWorld = m_mtxWorld * matTransform;
  }

 protected:
  Matrix m_mtxWorld;
};

typedef vector<Object3d*> Object3dPtrs;
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_SCENE_OBJECT_H
