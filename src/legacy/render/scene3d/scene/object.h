// Copyright (c) 2010 CCL. All rights reserved.
#ifndef LEGACY_RENDER_SCENE3D_SCENE_OBJECT_H
#define LEGACY_RENDER_SCENE3D_SCENE_OBJECT_H

#include "legacy/render/rhi3d/public/device/base.h"

namespace render {

class Smt3DRenderDevice;
typedef Smt3DRenderDevice* LP3DRENDERDEVICE;

// Base for leftover drawables that participate in scene visibility / materials.
class Smt3DRenderable {
 public:
  Smt3DRenderable() { m_bVisible = true; }
  virtual ~Smt3DRenderable() {}

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

  inline SmtMaterial& GetMaterial() { return m_matMaterial; }
  inline void SetMaterial(SmtMaterial& matMaterial) {
    m_matMaterial = matMaterial;
  }

 protected:
  bool m_bVisible;
  double m_dbfTransparent;
  Aabb m_aAbb;
  SmtMaterial m_matMaterial;
  string m_strTexName;
};

// Base for leftover objects that own a model matrix and tick Update.
class Smt3DMovable {
 public:
  Smt3DMovable() {}
  virtual ~Smt3DMovable() {}

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
class Smt3DObject : public Smt3DRenderable, public Smt3DMovable {
 public:
  virtual long Init(Vector3& vPos, SmtMaterial& matMaterial,
                    const char* szTexName = "") {
    m_vOrgPos = vPos;
    m_matMaterial = matMaterial;
    m_strTexName = szTexName;
    m_mtxModel.identity();
    m_mtxWorld.identity();

    return SMT_ERR_NONE;
  }
  virtual long Create(LP3DRENDERDEVICE p3DRenderDevice) = 0;
  virtual long Render(LP3DRENDERDEVICE p3DRenderDevice) = 0;
  virtual long Destroy() = 0;

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

typedef vector<Smt3DObject*> vSmt3DObjectPtrs;
}  // namespace render

#endif  // LEGACY_RENDER_SCENE3D_SCENE_OBJECT_H
