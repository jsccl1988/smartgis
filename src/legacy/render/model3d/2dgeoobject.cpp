#include "legacy/render/model3d/2dgeoobject.h"

#include <math.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "algorithm/geo/geometry.h"
#include "algorithm/tin/tin.h"
#include "base/core/bas_struct.h"
#include "legacy/render/render3d/statesmanager.h"

using namespace render;

namespace {

// Leftover 3D only needs a recognizable fill. Dense prefecture rings blow
// constrained TIN; stride-downsample keeps the outline and a cheap fan.
constexpr int kMaxTessRingVerts = 64;

int downsample_ring(const RawPoint *src, int n, RawPoint *dst, int max_verts) {
  if (!src || !dst || n < 3 || max_verts < 3) {
    return 0;
  }
  if (n <= max_verts) {
    for (int i = 0; i < n; ++i) {
      dst[i] = src[i];
    }
    return n;
  }
  const int step = (n + max_verts - 2) / (max_verts - 1);
  int out = 0;
  for (int i = 0; i < n && out < max_verts - 1; i += step) {
    dst[out++] = src[i];
  }
  dst[out++] = src[n - 1];
  return out;
}

double ring_bbox_area(const RawPoint *p, int n) {
  if (!p || n < 3) {
    return 0.0;
  }
  double minx = p[0].x;
  double maxx = p[0].x;
  double miny = p[0].y;
  double maxy = p[0].y;
  for (int i = 1; i < n; ++i) {
    minx = (std::min)(minx, p[i].x);
    maxx = (std::max)(maxx, p[i].x);
    miny = (std::min)(miny, p[i].y);
    maxy = (std::max)(maxy, p[i].y);
  }
  return (maxx - minx) * (maxy - miny);
}

}  // namespace

namespace render {
Smt2DGeoObject::Smt2DGeoObject(void)
    : m_pGeom(NULL),
      m_pStyle(NULL),
      m_pVertexBuffer(NULL),
      m_pIndexBuffer(NULL),
      m_height_fn(NULL),
      m_height_user(NULL) {}

void Smt2DGeoObject::SetHeightSampleFn(HeightSampleFn fn, void *user) {
  m_height_fn = fn;
  m_height_user = user;
}

ulong Smt2DGeoObject::vertex_format() const {
  ulong fmt = VF_XYZ | VF_DIFFUSE;
  if (m_height_fn) {
    fmt |= VF_NORMAL;
  }
  return fmt;
}

float Smt2DGeoObject::height_at(double x, double y) const {
  if (m_height_fn) {
    return m_height_fn(x, y, m_height_user);
  }
  return 0.f;
}

void Smt2DGeoObject::emit_map_vertex(SmtVertexBuffer *vb, double x, double y,
                                     float r, float g, float b) {
  if (!vb) {
    return;
  }
  const float h = height_at(x, y);
  if (m_height_fn) {
    const float eps = 0.12f;
    const float hx0 = height_at(x - eps, y);
    const float hx1 = height_at(x + eps, y);
    const float hy0 = height_at(x, y - eps);
    const float hy1 = height_at(x, y + eps);
    // X=-lon: flip the lon-space ∂h/∂x into world X.
    float nx = -(hx0 - hx1);
    float ny = 2.f * eps;
    float nz = hy0 - hy1;
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 1e-8f) {
      nx /= len;
      ny /= len;
      nz /= len;
    } else {
      nx = 0.f;
      ny = 1.f;
      nz = 0.f;
    }
    vb->Normal(nx, ny, nz);
  }
  // Geographic lon/lat in |x|/|y|; mesh X is -lon for east-on-right framing.
  vb->Vertex(static_cast<float>(-x), h, static_cast<float>(y));
  vb->Diffuse(r, g, b, 1.f);
}

Smt2DGeoObject::~Smt2DGeoObject() { Destroy(); }

long Smt2DGeoObject::Init(Vector3 &vPos, SmtMaterial &matMaterial,
                          const char *szTexName) {
  return Smt3DObject::Init(vPos, matMaterial, szTexName);
}

long Smt2DGeoObject::Create(LP3DRENDERDEVICE p3DRenderDevice) {
  if (NULL == p3DRenderDevice || NULL == m_pGeom) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (m_pStyle == NULL) {
    SmtStyle fallback;
    SetStyle(&fallback);
  }

  //.. Create VB
  switch (wkbFlatten(m_pGeom->getGeometryType())) {
    case wkbPoint:
      CreatePointVB(p3DRenderDevice, (OGRPoint *)m_pGeom);
      break;
    case wkbMultiPoint:
      CreateMultiPointVB(p3DRenderDevice, (OGRMultiPoint *)m_pGeom);
      break;
    case wkbLineString:
      CreateLineStringVB(p3DRenderDevice, (OGRLineString *)m_pGeom);
      break;
    case wkbLinearRing:
      CreateLinearRingVB(p3DRenderDevice, (OGRLinearRing *)m_pGeom);
      break;
    case wkbMultiLineString:
      CreateMultiLineStringVB(p3DRenderDevice, (OGRMultiLineString *)m_pGeom);
      break;
    case wkbPolygon:
      if (!CreatePolygonVB(p3DRenderDevice, (OGRPolygon *)m_pGeom)) {
        return SMT_ERR_FAILURE;
      }
      break;
    case wkbMultiPolygon:
      if (!CreateMultiPolygonVB(p3DRenderDevice, (OGRMultiPolygon *)m_pGeom)) {
        return SMT_ERR_FAILURE;
      }
      break;

    default:
      break;
  }

  Envelope env;
  copy_envelope(*m_pGeom, &env);

  const float h00 = height_at(env.MinX, env.MinY);
  const float h10 = height_at(env.MaxX, env.MinY);
  const float h01 = height_at(env.MinX, env.MaxY);
  const float h11 = height_at(env.MaxX, env.MaxY);
  const float hmin = (std::min)((std::min)(h00, h10), (std::min)(h01, h11));
  const float hmax = (std::max)((std::max)(h00, h10), (std::max)(h01, h11));
  m_aAbb.merge(-env.MaxX, hmin, env.MinY);
  m_aAbb.merge(-env.MinX, hmax, env.MaxY);

  m_aAbb.vcMax += m_vOrgPos;
  m_aAbb.vcMin += m_vOrgPos;

  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.;

  return SMT_ERR_NONE;
}

long Smt2DGeoObject::Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) {
  return SMT_ERR_NONE;
}

long Smt2DGeoObject::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (NULL == p3DRenderDevice || NULL == m_pVertexBuffer) {
    return SMT_ERR_INVALID_PARAM;
  }

  p3DRenderDevice->SetBackfaceCulling(RSV_CULL_NONE);
  p3DRenderDevice->SetShadeMode(RSV_SHADE_SOLID, 0,
                                SmtColor(1.f, 1.f, 1.f, 1.f));
  if (SmtGPUStateManager *states = p3DRenderDevice->GetStateManager()) {
    states->SetLight(m_height_fn != NULL);
    states->Set2DTextures(false);
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
      RenderPointVB(p3DRenderDevice);

      break;
    case wkbMultiPoint:
      RenderMultiPointVB(p3DRenderDevice);

      break;
    case wkbLineString:
      RenderLineStringVB(p3DRenderDevice);

      break;
    case wkbLinearRing:
      RenderLinearRingVB(p3DRenderDevice);

      break;
    case wkbMultiLineString:
      RenderMultiLineStringVB(p3DRenderDevice);

      break;
    case wkbPolygon:
    case wkbMultiPolygon:
      RenderPolygonVB(p3DRenderDevice);

      break;

    default:
      break;
  }

  p3DRenderDevice->MatrixPop();
  p3DRenderDevice->MatrixPop();

  return SMT_ERR_NONE;
}

bool Smt2DGeoObject::Select(LP3DRENDERDEVICE p3DRenderDevice,
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
      OGRPoint oPoint(vTar.x, vTar.z);
      switch (wkbFlatten(m_pGeom->getGeometryType())) {
        case wkbPolygon: {
          return m_pGeom->Contains(&oPoint);
        } break;
      }

      return false;
    }
  }

  return false;
}

long Smt2DGeoObject::Destroy() {
  // Release VB memory
  SMT_SAFE_DELETE(m_pVertexBuffer);
  SMT_SAFE_DELETE(m_pGeom);
  SMT_SAFE_DELETE(m_pIndexBuffer);

  return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
void Smt2DGeoObject::SetGeometryDirectly(OGRGeometry *pGeom) {
  SMT_SAFE_DELETE(m_pGeom);
  m_pGeom = pGeom;
}

void Smt2DGeoObject::SetGeometry(OGRGeometry *pGeom) {
  SMT_SAFE_DELETE(m_pGeom);

  if (pGeom != NULL)
    m_pGeom = pGeom->clone();
  else
    m_pGeom = NULL;
}

void Smt2DGeoObject::SetStyle(const SmtStyle *pStyle) {
  if (pStyle == NULL) return;

  SMT_SAFE_DELETE(m_pStyle);

  m_pStyle = pStyle->clone(pStyle->get_style_name());
}

Smt2DGeoObject *Smt2DGeoObject::Clone() {
  Smt2DGeoObject *pObj = new Smt2DGeoObject();
  if (pObj == NULL) return NULL;

  pObj->SetGeometry(m_pGeom);
  pObj->SetHeightSampleFn(m_height_fn, m_height_user);

  return pObj;
}

//////////////////////////////////////////////////////////////////////////
bool Smt2DGeoObject::CreatePointVB(LP3DRENDERDEVICE p3DRenderDevice,
                                   OGRPoint *pPoint) {
  SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

  m_pVertexBuffer =
      p3DRenderDevice->CreateVertexBuffer(1, vertex_format(), false);

  m_pVertexBuffer->Lock();

  emit_map_vertex(m_pVertexBuffer, pPoint->getX(), pPoint->getY(),
                  GetRValue(penDesc.lPenColor) / 255.f,
                  GetGValue(penDesc.lPenColor) / 255.f,
                  GetBValue(penDesc.lPenColor) / 255.f);

  m_pVertexBuffer->Unlock();
  return true;
}

bool Smt2DGeoObject::CreateMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,
                                        OGRMultiPoint *pMultPoint) {
  SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
      pMultPoint->getNumGeometries(), vertex_format(), false);

  m_pVertexBuffer->Lock();

  for (int i = 0; i < pMultPoint->getNumGeometries(); i++) {
    OGRPoint *pPoint = (OGRPoint *)pMultPoint->getGeometryRef(i);

    emit_map_vertex(m_pVertexBuffer, pPoint->getX(), pPoint->getY(),
                    GetRValue(penDesc.lPenColor) / 255.f,
                    GetGValue(penDesc.lPenColor) / 255.f,
                    GetBValue(penDesc.lPenColor) / 255.f);
  }

  m_pVertexBuffer->Unlock();

  return true;
}

bool Smt2DGeoObject::CreateLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,
                                        OGRLineString *pLineString) {
  SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
      pLineString->getNumPoints(), vertex_format(), false);

  m_pVertexBuffer->Lock();

  for (int i = 0; i < pLineString->getNumPoints(); i++) {
    emit_map_vertex(m_pVertexBuffer, pLineString->getX(i), pLineString->getY(i),
                    GetRValue(penDesc.lPenColor) / 255.f,
                    GetGValue(penDesc.lPenColor) / 255.f,
                    GetBValue(penDesc.lPenColor) / 255.f);
  }

  m_pVertexBuffer->Unlock();

  return true;
}

bool Smt2DGeoObject::CreateSplineVB(LP3DRENDERDEVICE p3DRenderDevice,
                                    OGRLineString *pSpline) {
  SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(pSpline->getNumPoints(),
                                                        vertex_format(), false);

  m_pVertexBuffer->Lock();

  for (int i = 0; i < pSpline->getNumPoints(); i++) {
    emit_map_vertex(m_pVertexBuffer, pSpline->getX(i), pSpline->getY(i),
                    GetRValue(penDesc.lPenColor) / 255.f,
                    GetGValue(penDesc.lPenColor) / 255.f,
                    GetBValue(penDesc.lPenColor) / 255.f);
  }

  m_pVertexBuffer->Unlock();

  return true;
}

bool Smt2DGeoObject::CreateLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,
                                        OGRLinearRing *pLinearRing) {
  SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
      pLinearRing->getNumPoints(), vertex_format(), false);

  m_pVertexBuffer->Lock();

  for (int i = 0; i < pLinearRing->getNumPoints(); i++) {
    emit_map_vertex(m_pVertexBuffer, pLinearRing->getX(i), pLinearRing->getY(i),
                    GetRValue(penDesc.lPenColor) / 255.f,
                    GetGValue(penDesc.lPenColor) / 255.f,
                    GetBValue(penDesc.lPenColor) / 255.f);
  }

  m_pVertexBuffer->Unlock();

  return true;
}

bool Smt2DGeoObject::CreateMultiLineStringVB(
    LP3DRENDERDEVICE p3DRenderDevice, OGRMultiLineString *pMultLinearRing) {
  SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

  int nDotNum = 0;
  for (int i = 0; i < pMultLinearRing->getNumGeometries(); i++) {
    OGRLineString *pLineString =
        (OGRLineString *)pMultLinearRing->getGeometryRef(i);
    nDotNum += pLineString->getNumPoints();
  }

  //////////////////////////////////////////////////////////////////////////
  m_pVertexBuffer =
      p3DRenderDevice->CreateVertexBuffer(nDotNum, vertex_format(), false);

  m_pVertexBuffer->Lock();

  for (int i = 0; i < pMultLinearRing->getNumGeometries(); i++) {
    OGRLineString *pLineString =
        (OGRLineString *)pMultLinearRing->getGeometryRef(i);
    for (int k = 0; k < pLineString->getNumPoints(); k++) {
      emit_map_vertex(m_pVertexBuffer, pLineString->getX(k),
                      pLineString->getY(k),
                      GetRValue(penDesc.lPenColor) / 255.f,
                      GetGValue(penDesc.lPenColor) / 255.f,
                      GetBValue(penDesc.lPenColor) / 255.f);
    }
  }

  m_pVertexBuffer->Unlock();

  return true;
}

bool Smt2DGeoObject::CreatePolygonVB(LP3DRENDERDEVICE p3DRenderDevice,
                                     OGRPolygon *pPolygon) {
  if (!p3DRenderDevice || !pPolygon || !m_pStyle) {
    return false;
  }
  SmtBrushDesc &brushDesc = m_pStyle->get_brush_desc();

  OGRLinearRing *pLinearRing = pPolygon->getExteriorRing();
  if (!pLinearRing) {
    return false;
  }
  int nPoints = pLinearRing->getNumPoints();
  if (nPoints < 3) return false;

  RawPoint *pRawPoints = new RawPoint[nPoints];
  for (int i = 0; i < nPoints; ++i) {
    pRawPoints[i].x = pLinearRing->getX(i);
    pRawPoints[i].y = pLinearRing->getY(i);
  }
  if (nPoints >= 4 && pRawPoints[0].x == pRawPoints[nPoints - 1].x &&
      pRawPoints[0].y == pRawPoints[nPoints - 1].y) {
    --nPoints;
  }
  if (nPoints < 3) {
    SMT_SAFE_DELETE_A(pRawPoints);
    return false;
  }
  RawPoint slim[kMaxTessRingVerts];
  const int slim_n =
      downsample_ring(pRawPoints, nPoints, slim, kMaxTessRingVerts);
  if (slim_n < 3) {
    SMT_SAFE_DELETE_A(pRawPoints);
    return false;
  }
  SMT_SAFE_DELETE_A(pRawPoints);
  nPoints = slim_n;
  pRawPoints = new RawPoint[nPoints];
  for (int i = 0; i < nPoints; ++i) {
    pRawPoints[i] = slim[i];
  }

  vector<SmtTriangle> vTriMesh;
  if (SMT_ERR_NONE !=
      divide_polygon_into_tri_mesh(vTriMesh, pRawPoints, nPoints)) {
    SMT_SAFE_DELETE_A(pRawPoints);
    return false;
  }

  vector<SmtTriangle> in_range;
  in_range.reserve(vTriMesh.size());
  for (const SmtTriangle &t : vTriMesh) {
    if (t.a >= 0 && t.b >= 0 && t.c >= 0 && t.a < nPoints && t.b < nPoints &&
        t.c < nPoints) {
      in_range.push_back(t);
    }
  }
  if (in_range.empty()) {
    SMT_SAFE_DELETE_A(pRawPoints);
    return false;
  }

  m_pVertexBuffer =
      p3DRenderDevice->CreateVertexBuffer(nPoints, vertex_format(), false);
  if (!m_pVertexBuffer) {
    SMT_SAFE_DELETE_A(pRawPoints);
    return false;
  }

  m_pVertexBuffer->Lock();

  for (int i = 0; i < nPoints; i++) {
    emit_map_vertex(m_pVertexBuffer, pRawPoints[i].x, pRawPoints[i].y,
                    GetRValue(brushDesc.lBrushColor) / 255.f,
                    GetGValue(brushDesc.lBrushColor) / 255.f,
                    GetBValue(brushDesc.lBrushColor) / 255.f);
  }

  m_pIndexBuffer = p3DRenderDevice->CreateIndexBuffer(in_range.size() * 3);
  if (!m_pIndexBuffer) {
    m_pVertexBuffer->Unlock();
    SMT_SAFE_DELETE(m_pVertexBuffer);
    SMT_SAFE_DELETE_A(pRawPoints);
    return false;
  }
  m_pIndexBuffer->Lock();

  for (size_t i = 0; i < in_range.size(); i++) {
    m_pIndexBuffer->Index(in_range[i].a);
    m_pIndexBuffer->Index(in_range[i].b);
    m_pIndexBuffer->Index(in_range[i].c);
  }

  m_pIndexBuffer->Unlock();

  m_pVertexBuffer->Unlock();

  SMT_SAFE_DELETE_A(pRawPoints);

  return true;
}

bool Smt2DGeoObject::CreateMultiPolygonVB(LP3DRENDERDEVICE p3DRenderDevice,
                                          OGRMultiPolygon *pMulti) {
  if (!p3DRenderDevice || !pMulti || !m_pStyle) {
    return false;
  }
  if (pMulti->getNumGeometries() == 1) {
    return CreatePolygonVB(p3DRenderDevice,
                           (OGRPolygon *)pMulti->getGeometryRef(0));
  }

  SmtBrushDesc &brushDesc = m_pStyle->get_brush_desc();
  std::vector<RawPoint> verts;
  std::vector<int> indices;
  const int ngeom = pMulti->getNumGeometries();
  double max_part_area = 0.0;
  for (int gi = 0; gi < ngeom; ++gi) {
    OGRPolygon *poly = (OGRPolygon *)pMulti->getGeometryRef(gi);
    OGRLinearRing *ring = poly ? poly->getExteriorRing() : nullptr;
    if (!ring || ring->getNumPoints() < 3) {
      continue;
    }
    OGREnvelope env;
    ring->getEnvelope(&env);
    const double area = (env.MaxX - env.MinX) * (env.MaxY - env.MinY);
    if (area > max_part_area) {
      max_part_area = area;
    }
  }
  for (int gi = 0; gi < ngeom; ++gi) {
    OGRPolygon *poly = (OGRPolygon *)pMulti->getGeometryRef(gi);
    if (!poly) {
      continue;
    }
    OGRLinearRing *ring = poly->getExteriorRing();
    if (!ring) {
      continue;
    }
    const int nPoints = ring->getNumPoints();
    if (nPoints < 3) {
      continue;
    }
    OGREnvelope env;
    ring->getEnvelope(&env);
    const double area = (env.MaxX - env.MinX) * (env.MaxY - env.MinY);
    // Drop speck islands so leftover 3D stays a China-like fill.
    if (max_part_area > 0.0 && area < max_part_area * 0.02) {
      continue;
    }
    int n = nPoints;
    std::vector<RawPoint> raw(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
      raw[static_cast<std::size_t>(i)].x = ring->getX(i);
      raw[static_cast<std::size_t>(i)].y = ring->getY(i);
    }
    if (n >= 4 && raw[0].x == raw[static_cast<std::size_t>(n - 1)].x &&
        raw[0].y == raw[static_cast<std::size_t>(n - 1)].y) {
      --n;
      raw.resize(static_cast<std::size_t>(n));
    }
    RawPoint slim[kMaxTessRingVerts];
    const int slim_n = downsample_ring(raw.data(), n, slim, kMaxTessRingVerts);
    if (slim_n < 3) {
      continue;
    }
    raw.assign(slim, slim + slim_n);
    n = slim_n;
    std::vector<SmtTriangle> tris;
    if (divide_polygon_into_tri_mesh(tris, raw.data(), n) != SMT_ERR_NONE) {
      continue;
    }
    const int base = static_cast<int>(verts.size());
    verts.insert(verts.end(), raw.begin(), raw.end());
    for (const SmtTriangle &t : tris) {
      if (t.a < 0 || t.b < 0 || t.c < 0 || t.a >= n || t.b >= n || t.c >= n) {
        continue;
      }
      indices.push_back(base + t.a);
      indices.push_back(base + t.b);
      indices.push_back(base + t.c);
    }
  }
  if (verts.empty() || indices.empty()) {
    return false;
  }

  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
      static_cast<int>(verts.size()), vertex_format(), false);
  if (!m_pVertexBuffer) {
    return false;
  }
  m_pVertexBuffer->Lock();
  const float cr = GetRValue(brushDesc.lBrushColor) / 255.f;
  const float cg = GetGValue(brushDesc.lBrushColor) / 255.f;
  const float cb = GetBValue(brushDesc.lBrushColor) / 255.f;
  for (const RawPoint &p : verts) {
    emit_map_vertex(m_pVertexBuffer, p.x, p.y, cr, cg, cb);
  }
  m_pIndexBuffer =
      p3DRenderDevice->CreateIndexBuffer(static_cast<int>(indices.size()));
  if (!m_pIndexBuffer) {
    m_pVertexBuffer->Unlock();
    return false;
  }
  m_pIndexBuffer->Lock();
  for (int ix : indices) {
    m_pIndexBuffer->Index(ix);
  }
  m_pIndexBuffer->Unlock();
  m_pVertexBuffer->Unlock();
  return true;
}

//////////////////////////////////////////////////////////////////////////
bool Smt2DGeoObject::RenderPointVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_POINTLIST, m_pVertexBuffer, 0, 1);

  return true;
}

bool Smt2DGeoObject::RenderMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_POINTLIST, m_pVertexBuffer, 0,
                                  m_pVertexBuffer->GetVertexCount());

  return true;
}

bool Smt2DGeoObject::RenderLineStringVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_LINESTRIP, m_pVertexBuffer, 0,
                                  m_pVertexBuffer->GetVertexCount());

  return true;
}

bool Smt2DGeoObject::RenderSplineVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_LINESTRIP, m_pVertexBuffer, 0,
                                  m_pVertexBuffer->GetVertexCount());

  return true;
}

bool Smt2DGeoObject::RenderLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_LINESTRIP, m_pVertexBuffer, 0,
                                  m_pVertexBuffer->GetVertexCount());

  return true;
}

bool Smt2DGeoObject::RenderMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice) {
  p3DRenderDevice->DrawPrimitives(PT_LINESTRIP, m_pVertexBuffer, 0,
                                  m_pVertexBuffer->GetVertexCount());

  return true;
}

bool Smt2DGeoObject::RenderPolygonVB(LP3DRENDERDEVICE p3DRenderDevice) {
  if (!p3DRenderDevice || !m_pVertexBuffer || !m_pIndexBuffer) {
    return false;
  }
  const ulong nidx = m_pIndexBuffer->GetIndexCount();
  if (nidx < 3) {
    return false;
  }
  p3DRenderDevice->DrawIndexedPrimitives(PT_TRIANGLELIST, m_pVertexBuffer,
                                         m_pIndexBuffer, 0, nidx / 3);

  return true;
}
}  // namespace render