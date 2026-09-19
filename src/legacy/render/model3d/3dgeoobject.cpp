#include "legacy/render/model3d/3dgeoobject.h"

#include <math.h>

#include "algorithm/geo/geometry.h"
#include "base/core/bas_struct.h"
#include "render/math/math.h"

using namespace render;

namespace render {
Smt3DGeoObject::Smt3DGeoObject(void)
    : m_pGeom(NULL), m_pVertexBuffer(NULL), m_pIndexBuffer(NULL) {}

Smt3DGeoObject::~Smt3DGeoObject() { Destroy(); }

long Smt3DGeoObject::Init(Vector3 &vPos, SmtMaterial &matMaterial,
                          const char *szTexName) {
  return Smt3DObject::Init(vPos, matMaterial, szTexName);
}

long Smt3DGeoObject::Create(LP3DRENDERDEVICE p3DRenderDevice) {
  if (NULL == p3DRenderDevice || NULL == m_pGeom) return SMT_ERR_INVALID_PARAM;

  //.. Create VB
  switch (wkbFlatten(m_pGeom->getGeometryType())) {
    case wkbPoint:
      Create3DPointVB(p3DRenderDevice, (OGRPoint *)m_pGeom);

      break;
    case wkbMultiPoint:
      Create3DMultiPointVB(p3DRenderDevice, (OGRMultiPoint *)m_pGeom);

      break;
    case wkbLineString:
      Create3DLineStringVB(p3DRenderDevice, (OGRLineString *)m_pGeom);

      break;
    case wkbLinearRing:
      Create3DLinearRingVB(p3DRenderDevice, (OGRLinearRing *)m_pGeom);

      break;
    case wkbMultiLineString:
      Create3DMultiLineStringVB(p3DRenderDevice, (OGRMultiLineString *)m_pGeom);

      break;

    default:
      break;
  }

  OGREnvelope3D env;
  copy_envelope3d(*m_pGeom, &env);
  m_aAbb.vcMin.set(static_cast<float>(env.MinX), static_cast<float>(env.MinY),
                   static_cast<float>(env.MinZ));
  m_aAbb.vcMax.set(static_cast<float>(env.MaxX), static_cast<float>(env.MaxY),
                   static_cast<float>(env.MaxZ));

  m_aAbb.vcMax += m_vOrgPos;
  m_aAbb.vcMin += m_vOrgPos;

  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.;

  return SMT_ERR_NONE;
}

long Smt3DGeoObject::Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) {
  return SMT_ERR_NONE;
}

long Smt3DGeoObject::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (NULL == p3DRenderDevice || NULL == m_pVertexBuffer) {
    return SMT_ERR_INVALID_PARAM;
  }

  SmtTexture *pTex = p3DRenderDevice->GetTexture(m_strTexName.c_str());
  if (pTex) p3DRenderDevice->SetTexture(pTex);

  p3DRenderDevice->SetMaterial(&m_matMaterial);

  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxWorld);

  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxModel);

  // render
  switch (wkbFlatten(m_pGeom->getGeometryType())) {
    case wkbPoint:
      Render3DPointVB(p3DRenderDevice);
      break;
    case wkbMultiPoint:
      Render3DMultiPointVB(p3DRenderDevice);
      break;
    case wkbLineString:
      Render3DLineStringVB(p3DRenderDevice);
      break;
    case wkbLinearRing:
      Render3DLinearRingVB(p3DRenderDevice);
      break;
    case wkbMultiLineString:
      Render3DMultiLineStringVB(p3DRenderDevice);
      break;
    default:
      break;
  }

  p3DRenderDevice->MatrixPop();
  p3DRenderDevice->MatrixPop();

  return SMT_ERR_NONE;
}

bool Smt3DGeoObject::Select(LP3DRENDERDEVICE p3DRenderDevice,
                            const lPoint &point) {
  if (NULL == p3DRenderDevice || NULL == m_pVertexBuffer) return false;

  Vector3 vOrg, vTar, vDir;

  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxWorld);

  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxModel);
  p3DRenderDevice->Transform2DTo3D(vOrg, vTar, point);
  p3DRenderDevice->MatrixPop();

  p3DRenderDevice->MatrixPop();

  vDir = vTar - vOrg;
  if (vDir.length_squared() > 0) {
    Ray ray;
    float f;
    ray.set(vOrg, vDir);
    if (ray.intersects(m_aAbb, &f)) {
      return true;
    }
  }

  return false;
}

long Smt3DGeoObject::Destroy() {
  // Release VB memory
  SMT_SAFE_DELETE(m_pVertexBuffer);
  SMT_SAFE_DELETE(m_pGeom);
  SMT_SAFE_DELETE(m_pIndexBuffer);

  return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
void Smt3DGeoObject::SetGeometryDirectly(OGRGeometry *pGeom) {
  SMT_SAFE_DELETE(m_pGeom);
  m_pGeom = pGeom;
}

void Smt3DGeoObject::SetGeometry(OGRGeometry *pGeom) {
  SMT_SAFE_DELETE(m_pGeom);

  if (pGeom != NULL)
    m_pGeom = pGeom->clone();
  else
    m_pGeom = NULL;
}

Smt3DGeoObject *Smt3DGeoObject::Clone() {
  Smt3DGeoObject *pObj = new Smt3DGeoObject();
  if (pObj == NULL) return NULL;

  pObj->SetGeometry(m_pGeom);

  return pObj;
}

//////////////////////////////////////////////////////////////////////////
bool Smt3DGeoObject::Create3DPointVB(LP3DRENDERDEVICE p3DRenderDevice,
                                     OGRPoint *p3DPoint) {
  m_pVertexBuffer =
      p3DRenderDevice->CreateVertexBuffer(1, VF_XYZ | VF_DIFFUSE, false);

  m_pVertexBuffer->Lock();

  m_pVertexBuffer->Vertex(p3DPoint->getX(), p3DPoint->getZ(), p3DPoint->getY());

  m_pVertexBuffer->Unlock();

  return true;
}

bool Smt3DGeoObject::Create3DMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,
                                          OGRMultiPoint *p3DMultPoint) {
  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
      p3DMultPoint->getNumGeometries(), VF_XYZ, false);

  m_pVertexBuffer->Lock();

  for (int i = 0; i < p3DMultPoint->getNumGeometries(); i++) {
    OGRPoint *p3DPoint = (OGRPoint *)p3DMultPoint->getGeometryRef(i);
    m_pVertexBuffer->Vertex(p3DPoint->getX(), p3DPoint->getZ(),
                            p3DPoint->getY());
  }

  m_pVertexBuffer->Unlock();

  return true;
}

bool Smt3DGeoObject::Create3DLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,
                                          OGRLineString *p3DLineString) {
  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
      p3DLineString->getNumPoints(), VF_XYZ, false);

  m_pVertexBuffer->Lock();

  for (int i = 0; i < p3DLineString->getNumPoints(); i++) {
    m_pVertexBuffer->Vertex(p3DLineString->getX(i), p3DLineString->getZ(i),
                            p3DLineString->getY(i));
  }

  m_pVertexBuffer->Unlock();

  return true;
}

bool Smt3DGeoObject::Create3DLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,
                                          OGRLinearRing *p3DLinearRing) {
  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
      p3DLinearRing->getNumPoints(), VF_XYZ, false);

  m_pVertexBuffer->Lock();

  for (int i = 0; i < p3DLinearRing->getNumPoints(); i++) {
    m_pVertexBuffer->Vertex(p3DLinearRing->getX(i), p3DLinearRing->getZ(i),
                            p3DLinearRing->getY(i));
  }

  m_pVertexBuffer->Unlock();

  return true;
}

bool Smt3DGeoObject::Create3DMultiLineStringVB(
    LP3DRENDERDEVICE p3DRenderDevice, OGRMultiLineString *p3DMultLinearRing) {
  return true;
}

bool Smt3DGeoObject::Create3DSurfaceVB(LP3DRENDERDEVICE p3DRenderDevice,
                                       Smt3DSurface *p3DSurf) {
  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
      p3DSurf->get_point_count(), VF_XYZ | VF_TEXCOORD | VF_NORMAL, false);

  m_pVertexBuffer->Lock();

  for (int i = 0; i < p3DSurf->get_point_count(); i++) {
    OGRPoint point = p3DSurf->get_point(i);
    m_pVertexBuffer->Vertex(point.getX(), point.getZ(), point.getY());
  }

  m_pIndexBuffer =
      p3DRenderDevice->CreateIndexBuffer(p3DSurf->get_triangle_count() * 3);
  m_pIndexBuffer->Lock();

  for (int i = 0; i < p3DSurf->get_triangle_count(); i++) {
    Smt3DTriangle tri = p3DSurf->get_triangle(i);
    m_pIndexBuffer->Index(tri.a);
    m_pIndexBuffer->Index(tri.b);
    m_pIndexBuffer->Index(tri.c);
  }

  m_pIndexBuffer->Unlock();

  // calculator TextureCoord
  Aabb aabb;
  OGREnvelope3D env;
  p3DSurf->get_envelope(&env);
  aabb.merge(env.MinX, env.MinY, env.MinZ);
  aabb.merge(env.MaxX, env.MaxY, env.MaxZ);

  float fXDis = aabb.vcMax.x - aabb.vcMin.x,
        fYDis = aabb.vcMax.y - aabb.vcMin.y;
  for (int i = 0; i < p3DSurf->get_point_count(); i++) {
    OGRPoint point = p3DSurf->get_point(i);
    m_pVertexBuffer->TexVertex((point.getX() - aabb.vcMin.x) / fXDis * 16,
                               (point.getY() - aabb.vcMin.y) / fYDis * 16);
  }

  // normal
  // new mem
  Vector4 *pNormals = new Vector4[p3DSurf->get_point_count()];

  // calculator normal
  for (int i = 0; i < p3DSurf->get_triangle_count(); i++) {
    Smt3DTriangle tri = p3DSurf->get_triangle(i);
    OGRPoint point1 = p3DSurf->get_point(tri.a);
    OGRPoint point2 = p3DSurf->get_point(tri.b);
    OGRPoint point3 = p3DSurf->get_point(tri.c);

    Vector4 V1(point1.getX(), point1.getY(), point1.getZ()),
        V2(point2.getX(), point2.getY(), point2.getZ()),
        V3(point3.getX(), point3.getY(), point3.getZ());

    Vector4 &n1 = pNormals[tri.a];
    Vector4 &n2 = pNormals[tri.b];
    Vector4 &n3 = pNormals[tri.c];

    Vector4 nor1 = triangle_normal(V1, V2, V3);

    n1 += nor1;
    n2 += nor1;
    n3 += nor1;
  }

  // normalize
  for (int i = 0; i < p3DSurf->get_point_count(); i++) {
    pNormals[i].normalize();
    m_pVertexBuffer->Normal(pNormals[i].x, pNormals[i].z, pNormals[i].y);
  }

  SMT_SAFE_DELETE_A(pNormals);

  m_pVertexBuffer->Unlock();

  return true;
}

//////////////////////////////////////////////////////////////////////////
bool Smt3DGeoObject::Render3DPointVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_POINTLIST, m_pVertexBuffer, 0, 1);

  return true;
}

bool Smt3DGeoObject::Render3DMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_POINTLIST, m_pVertexBuffer, 0,
                                  m_pVertexBuffer->GetVertexCount());

  return true;
}

bool Smt3DGeoObject::Render3DLineStringVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_LINESTRIP, m_pVertexBuffer, 0,
                                  m_pVertexBuffer->GetVertexCount());

  return true;
}

bool Smt3DGeoObject::Render3DLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_LINESTRIP, m_pVertexBuffer, 0,
                                  m_pVertexBuffer->GetVertexCount());

  return true;
}

bool Smt3DGeoObject::Render3DMultiLineStringVB(
    LP3DRENDERDEVICE p3DRenderDevice) {
  return true;
}

bool Smt3DGeoObject::Render3DSurfaceVB(LP3DRENDERDEVICE p3DRenderDevice) {
  Smt3DSurface *p3DSurf = (Smt3DSurface *)m_pGeom;
  p3DRenderDevice->DrawIndexedPrimitives(PT_TRIANGLELIST, m_pVertexBuffer,
                                         m_pIndexBuffer, 0,
                                         p3DSurf->get_triangle_count());

  return true;
}
}  // namespace render