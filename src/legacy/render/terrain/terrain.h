/*
File:   md3d_terrain.h

Desc:    SmtTerrain,����

Version: Version 1.0

Writter:  �´���

Date:    2011.8.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_TERRAIN_H
#define _MD3D_TERRAIN_H

#include "algorithm/geo/geometry.h"
#include "legacy/render/render3d/3drenderdevice.h"
#include "legacy/render/render3d/3drenderer.h"
#include "legacy/render/render3d/videobuffer.h"
#include "legacy/render/scene3d/bl3d_object.h"

#if !defined(TERRAIN_EXPORT_DEFINED)
#define TERRAIN_EXPORT_DEFINED
#if defined(TERRAIN_EXPORTS)
#define TERRAIN_EXPORT_API __declspec(dllexport)
#define TERRAIN_EXPORT_CLASS __declspec(dllexport)
#else
#define TERRAIN_EXPORT_API __declspec(dllimport)
#define TERRAIN_EXPORT_CLASS __declspec(dllimport)
#endif
#endif

using namespace render;
using namespace geo;

namespace render {
class TERRAIN_EXPORT_CLASS SmtTerrain : public Smt3DObject {
 public:
  SmtTerrain(void);
  virtual ~SmtTerrain(void);

 public:
  long Init(Vector3 &vPos, SmtMaterial &matMaterial,
            const char *szTexName = "");
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  bool Select(LP3DRENDERDEVICE p3DRenderDevice, const lPoint &point);
  long Destroy();

 public:
  inline Vector3 GetCenter() { return m_vCenter; }

  inline void SetClrType(int type) { m_nClrType = type; }
  inline void SetXScale(float fScale) { m_fXScale = fScale; }
  inline void SetYScale(float fScale) { m_fYScale = fScale; }
  inline void SetZScale(float fScale) { m_fZScale = fScale; }

  inline int GetClrType(void) { return m_nClrType; }
  inline float GetXScale(void) { return m_fXScale; }
  inline float GetYScale(void) { return m_fYScale; }
  inline float GetZScale(void) { return m_fZScale; }

 public:
  virtual Smt3DSurface *GetTerrainSurf(void) { return m_p3DSurf; }
  virtual long SetTerrainSurf(Smt3DSurface *pSurf);
  virtual long SetTerrainSurfDirectly(Smt3DSurface *pSurf);

 protected:
  virtual void CreateNormal(void);
  virtual void CreateTextureCoord(void);
  virtual void CreateColor(void);

  inline void GetColor(float h, SmtColor &clr);

 protected:
  void RenderTerrain(LP3DRENDERDEVICE p3DRenderDevice);

 protected:
  int m_nClrType;  // �ֲ���ɫʱ�õ�
  int m_nColorLevels;
  SmtColor *m_pClr;

  float m_fZScale;
  float m_fXScale;
  float m_fYScale;

  float m_fMinZ;
  float m_fMaxZ;
  Vector3 m_vCenter;  // �����е�����x,y,z

  Smt3DSurface *m_p3DSurf;

 protected:
  SmtVertexBuffer *m_pVertexBuffer;
  SmtIndexBuffer *m_pIndexBuffer;
};
}  // namespace render

#if !defined(TERRAIN_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_MD3D_TERRAIN_H