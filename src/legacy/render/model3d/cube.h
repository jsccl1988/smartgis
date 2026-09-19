/*
File:   md3d_cube.h

Desc:    SmtCube,����ģ��

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _MD3D_CUBE_H
#define _MD3D_CUBE_H

#include "legacy/render/render3d/3drenderdevice.h"
#include "legacy/render/render3d/3drenderer.h"
#include "legacy/render/render3d/videobuffer.h"
#include "legacy/render/scene3d/bl3d_object.h"

#if !defined(MODEL3D_EXPORT_DEFINED)
#define MODEL3D_EXPORT_DEFINED
#if defined(MODEL3D_EXPORTS)
#define MODEL3D_EXPORT_API __declspec(dllexport)
#define MODEL3D_EXPORT_CLASS __declspec(dllexport)
#else
#define MODEL3D_EXPORT_API __declspec(dllimport)
#define MODEL3D_EXPORT_CLASS __declspec(dllimport)
#endif
#endif

using namespace render;

namespace render {
class MODEL3D_EXPORT_CLASS SmtCube : public Smt3DObject {
 public:
  SmtCube(LP3DRENDERDEVICE pRenderDevice, Vector3 vCenter, float width);
  virtual ~SmtCube();

 public:
  long Init(Vector3& vPos, SmtMaterial& matMaterial,
            const char* szTexName = "");
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Destroy();

 private:
  SmtVertexBuffer* m_pVertexBuffer;
  Vector3 m_vCenter;
  float m_fWidth;
};
}  // namespace render

#if !defined(MODEL3D_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_MD3D_CUBE_H