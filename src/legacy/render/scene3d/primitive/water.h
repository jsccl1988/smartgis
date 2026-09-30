// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MD3D_WATER_H
#define _MD3D_WATER_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_device.h"
#include "legacy/render/rhi3d/public/device/renderer.h"
#include "legacy/render/rhi3d/public/resource/video_buffer.h"
#include "legacy/render/scene3d/scene/object.h"

using namespace render;

const double CST_DBF_MAX_DELTA = 0.01;
const double CST_DBF_ANIMATION_SPEED = 1.;

const int CST_INT_GRID_WIDTH = 128;
const int CST_INT_GRID_HEIGHT = 128;
const int CST_INT_GRID_SIZE = CST_INT_GRID_WIDTH * CST_INT_GRID_HEIGHT;

namespace render {
struct WVertex {
  float x, y, z;
  float nx, ny, nz;
  float u, v;
  float r, g, b;
};

class LEGACY_RENDER_EXPORT SmtWater : public Smt3DObject {
 public:
  SmtWater();
  virtual ~SmtWater();

 public:
  long Init(Vector3& vPos, SmtMaterial& matMaterial,
            const char* szTexName = "");
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Destroy();

  inline void SetXScale(float fScale) { m_fXScale = fScale; }
  inline void SetYScale(float fScale) { m_fYScale = fScale; }
  inline void SetZScale(float fScale) { m_fZScale = fScale; }

  inline float GetXScale(void) { return m_fXScale; }
  inline float GetYScale(void) { return m_fYScale; }
  inline float GetZScale(void) { return m_fZScale; }

 public:
  bool Select(LP3DRENDERDEVICE p3DRenderDevice, const lPoint& point);

 protected:
  void Compute(float dt = .1);
  void AdjustGrid(void);

  void UpdateVertex(void);
  void UpdateNormal(void);
  void UpdateTexcoord(void);

  void UpdateVB(void);

 private:
  SmtVertexBuffer* m_pVertexBuffer;
  Vector3 m_vCenter;
  float m_fWidth;
  float m_fTexOffset;

  float m_fZScale;
  float m_fXScale;
  float m_fYScale;

 private:
  double p[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT];
  double vx[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT],
      vy[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT];
  double ax[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT],
      ay[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT];
  WVertex wvertex[CST_INT_GRID_SIZE];
};
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_MD3D_WATER_H