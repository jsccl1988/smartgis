#include "scenic/scene3d/primitive/mesh/cube.h"

#include <math.h>

namespace scenic {
namespace detail {
Cube::Cube(LP3DRENDERDEVICE /*pRenderDevice*/, ::base::Vector3 vCenter,
                 float dbfWidth) {
  m_vCenter = vCenter;
  m_fWidth = dbfWidth;
}

Cube::~Cube() { Destroy(); }

long Cube::Init(::base::Vector3& vPos, Material& matMaterial,
                   const char* szTexName) {
  Object3d::Init(vPos, matMaterial, szTexName);

  m_matMaterial.SetAmbientValue(Color(0, 0, 0));
  m_matMaterial.SetDiffuseValue(Color(.1, .3, 1));
  m_matMaterial.SetSpecularValue(Color(1, 1, 1));
  m_matMaterial.SetEmissiveValue(Color(0.1, 1, 1, 1));
  m_matMaterial.SetShininessValue(22);

  return SMT_ERR_NONE;
}

long Cube::Create(LP3DRENDERDEVICE p3DRenderDevice) {
  if (NULL == p3DRenderDevice) {
    return SMT_ERR_INVALID_PARAM;
  }

  m_aAbb.vcMax = m_vCenter + (m_fWidth / 2.);
  m_aAbb.vcMin = m_vCenter - (m_fWidth / 2.);
  m_aAbb.vcCenter = m_vCenter;

  return SMT_ERR_NONE;
}

long Cube::Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) {
  return SMT_ERR_NONE;
}

long Cube::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (NULL == p3DRenderDevice) {
    return SMT_ERR_INVALID_PARAM;
  }

  p3DRenderDevice->SetMaterial(&m_matMaterial);

  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxWorld);

  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxModel);
  p3DRenderDevice->DrawCube3D(m_vCenter, m_fWidth, Color(1., 0., 0., 1.));
  p3DRenderDevice->MatrixPop();

  p3DRenderDevice->MatrixPop();

  return SMT_ERR_NONE;
}

long Cube::Destroy() {
  // Release VB memory
  SMT_SAFE_DELETE(m_pVertexBuffer);

  return SMT_ERR_NONE;
}
}  // namespace detail
}  // namespace scenic