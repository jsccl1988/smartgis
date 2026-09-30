// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/surface/pointcloud.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "legacy/render/rhi3d/public/device/base.h"

namespace render {
namespace {

// Above this count, vertices are reordered into spatial chunks for frustum
// culling. Smaller clouds keep a single full-VB draw.
constexpr int kChunkPointThreshold = 200000;
constexpr int kTargetChunkPoints = 16384;

ulong cell_key(int ix, int iy, int iz) {
  // 10 bits per axis is enough for leftover grid sizes.
  const ulong x = static_cast<ulong>(ix) & 0x3FFul;
  const ulong y = static_cast<ulong>(iy) & 0x3FFul;
  const ulong z = static_cast<ulong>(iz) & 0x3FFul;
  return (x << 20) | (y << 10) | z;
}

}  // namespace

Smt3DPointCloud::Smt3DPointCloud(void)
    : m_bReadOK(false),
      m_bShowBounds(true),
      m_pVertexBuffer(NULL),
      m_nLastDrawnPoints(0) {}

Smt3DPointCloud::~Smt3DPointCloud() { Destroy(); }

long Smt3DPointCloud::Init(Vector3& vPos, SmtMaterial& matMaterial) {
  return Smt3DObject::Init(vPos, matMaterial);
}

void Smt3DPointCloud::pack_vertices_for_chunks(SmtVertex3DList* packed) {
  if (!packed || m_vtxList.nCount < 1 || !m_vtxList.pVertexs) {
    return;
  }

  if (m_vtxList.nCount < kChunkPointThreshold) {
    *packed = m_vtxList;
    return;
  }

  Aabb bounds;
  for (int i = 0; i < m_vtxList.nCount; ++i) {
    bounds.merge(m_vtxList.pVertexs[i].ver);
  }
  const double ex =
      (std::max)(1.0e-6, static_cast<double>(bounds.vcMax.x - bounds.vcMin.x));
  const double ey =
      (std::max)(1.0e-6, static_cast<double>(bounds.vcMax.y - bounds.vcMin.y));
  const double ez =
      (std::max)(1.0e-6, static_cast<double>(bounds.vcMax.z - bounds.vcMin.z));

  int cells = static_cast<int>(
      std::cbrt(static_cast<double>(m_vtxList.nCount) / kTargetChunkPoints));
  cells = (std::max)(1, (std::min)(cells, 64));

  std::map<ulong, std::vector<int>> buckets;
  for (int i = 0; i < m_vtxList.nCount; ++i) {
    const Vector3& v = m_vtxList.pVertexs[i].ver;
    int ix = static_cast<int>((v.x - bounds.vcMin.x) / ex * cells);
    int iy = static_cast<int>((v.y - bounds.vcMin.y) / ey * cells);
    int iz = static_cast<int>((v.z - bounds.vcMin.z) / ez * cells);
    ix = (std::max)(0, (std::min)(ix, cells - 1));
    iy = (std::max)(0, (std::min)(iy, cells - 1));
    iz = (std::max)(0, (std::min)(iz, cells - 1));
    buckets[cell_key(ix, iy, iz)].push_back(i);
  }

  vSmtVertex3Ds ordered;
  ordered.reserve(static_cast<size_t>(m_vtxList.nCount));
  for (const auto& entry : buckets) {
    for (int idx : entry.second) {
      ordered.push_back(m_vtxList.pVertexs[idx]);
    }
  }
  *packed = ordered;
}

void Smt3DPointCloud::build_chunks(const SmtVertex3DList& packed) {
  m_chunks.clear();
  if (packed.nCount < 1 || !packed.pVertexs) {
    return;
  }

  if (packed.nCount < kChunkPointThreshold) {
    PointCloudChunk chunk;
    chunk.start = 0;
    chunk.count = static_cast<ulong>(packed.nCount);
    for (int i = 0; i < packed.nCount; ++i) {
      chunk.aabb.merge(packed.pVertexs[i].ver);
    }
    chunk.aabb.vcCenter = (chunk.aabb.vcMax + chunk.aabb.vcMin) / 2.;
    m_chunks.push_back(chunk);
    return;
  }

  Aabb bounds;
  for (int i = 0; i < packed.nCount; ++i) {
    bounds.merge(packed.pVertexs[i].ver);
  }
  const double ex =
      (std::max)(1.0e-6, static_cast<double>(bounds.vcMax.x - bounds.vcMin.x));
  const double ey =
      (std::max)(1.0e-6, static_cast<double>(bounds.vcMax.y - bounds.vcMin.y));
  const double ez =
      (std::max)(1.0e-6, static_cast<double>(bounds.vcMax.z - bounds.vcMin.z));
  int cells = static_cast<int>(
      std::cbrt(static_cast<double>(packed.nCount) / kTargetChunkPoints));
  cells = (std::max)(1, (std::min)(cells, 64));

  // Packed order is already contiguous by cell_key; rebuild ranges by walking.
  auto cell_of = [&](const Vector3& v) -> ulong {
    int ix = static_cast<int>((v.x - bounds.vcMin.x) / ex * cells);
    int iy = static_cast<int>((v.y - bounds.vcMin.y) / ey * cells);
    int iz = static_cast<int>((v.z - bounds.vcMin.z) / ez * cells);
    ix = (std::max)(0, (std::min)(ix, cells - 1));
    iy = (std::max)(0, (std::min)(iy, cells - 1));
    iz = (std::max)(0, (std::min)(iz, cells - 1));
    return cell_key(ix, iy, iz);
  };

  PointCloudChunk cur;
  cur.start = 0;
  cur.count = 0;
  ulong prev_key = cell_of(packed.pVertexs[0].ver);
  cur.aabb.merge(packed.pVertexs[0].ver);
  cur.count = 1;

  for (int i = 1; i < packed.nCount; ++i) {
    const ulong key = cell_of(packed.pVertexs[i].ver);
    if (key != prev_key) {
      cur.aabb.vcCenter = (cur.aabb.vcMax + cur.aabb.vcMin) / 2.;
      m_chunks.push_back(cur);
      cur = PointCloudChunk{};
      cur.start = static_cast<ulong>(i);
      cur.count = 0;
      prev_key = key;
    }
    cur.aabb.merge(packed.pVertexs[i].ver);
    ++cur.count;
  }
  cur.aabb.vcCenter = (cur.aabb.vcMax + cur.aabb.vcMin) / 2.;
  m_chunks.push_back(cur);
}

long Smt3DPointCloud::build_gpu_buffer(LP3DRENDERDEVICE p3DRenderDevice) {
  SmtVertex3DList packed;
  pack_vertices_for_chunks(&packed);
  if (packed.nCount < 1 || !packed.pVertexs) {
    return SMT_ERR_FAILURE;
  }

  // Keep CPU list in draw order so unibn indices match VB order.
  m_vtxList = packed;

  SMT_SAFE_DELETE(m_pVertexBuffer);
  m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
      m_vtxList.nCount, VF_XYZ | VF_DIFFUSE, false);
  if (!m_pVertexBuffer) {
    return SMT_ERR_FAILURE;
  }

  m_pVertexBuffer->Lock();
  for (int i = 0; i < m_vtxList.nCount; ++i) {
    Vector3& vPos = m_vtxList.pVertexs[i].ver;
    SmtColor& clr = m_vtxList.pVertexs[i].clr;
    m_pVertexBuffer->Vertex(vPos.x, vPos.y, vPos.z);
    m_pVertexBuffer->Diffuse(clr.fRed, clr.fGreen, clr.fBlue, clr.fA);
  }
  m_pVertexBuffer->Unlock();

  build_chunks(m_vtxList);
  return SMT_ERR_NONE;
}

long Smt3DPointCloud::Create(LP3DRENDERDEVICE p3DRenderDevice) {
  if (NULL == p3DRenderDevice) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (!m_bReadOK || m_vtxList.nCount < 1 || m_vtxList.pVertexs == NULL) {
    return SMT_ERR_FAILURE;
  }

  const long gpu_err = build_gpu_buffer(p3DRenderDevice);
  if (gpu_err != SMT_ERR_NONE) {
    return gpu_err;
  }

  m_aAbb = Aabb();
  for (int i = 0; i < m_vtxList.nCount; ++i) {
    m_aAbb.merge(m_vtxList.pVertexs[i].ver);
  }
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.;

  return m_point_index.build(m_vtxList);
}

long Smt3DPointCloud::Update(LP3DRENDERDEVICE /*p3DRenderDevice*/,
                             float /*fElapsed*/) {
  return SMT_ERR_NONE;
}

long Smt3DPointCloud::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (NULL == p3DRenderDevice) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!m_pVertexBuffer || m_chunks.empty()) {
    return SMT_ERR_NONE;
  }

  p3DRenderDevice->SetMaterial(&m_matMaterial);
  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxModel);

  SmtFrustum frustum;
  p3DRenderDevice->GetFrustum(frustum);

  if (m_bShowBounds) {
    SmtGPUStateManager* stateManager = p3DRenderDevice->GetStateManager();
    stateManager->SetLight(false);
    stateManager->Set2DTextures(false);
    const float width = static_cast<float>(
        (std::max)(m_aAbb.vcMax.x - m_aAbb.vcMin.x,
                   (std::max)(m_aAbb.vcMax.y - m_aAbb.vcMin.y,
                              m_aAbb.vcMax.z - m_aAbb.vcMin.z)));
    p3DRenderDevice->DrawCube3D(m_aAbb.vcCenter, width,
                                SmtColor(0., 1., 0., 1.));
    stateManager->SetLight(true);
    stateManager->Set2DTextures(true);
  }

  m_nLastDrawnPoints = 0;
  for (const PointCloudChunk& chunk : m_chunks) {
    Vector3 max = chunk.aabb.vcMax;
    Vector3 min = chunk.aabb.vcMin;
    if (!frustum.IsBoxIn(max, min)) {
      continue;
    }
    p3DRenderDevice->DrawPrimitives(PT_POINTLIST, m_pVertexBuffer, chunk.start,
                                    chunk.count);
    m_nLastDrawnPoints += static_cast<int>(chunk.count);
  }

  char szBuf[TEMP_BUFFER_SIZE];
  snprintf(szBuf, TEMP_BUFFER_SIZE, "points drawn:%d/%d chunks:%zu",
           m_nLastDrawnPoints, m_vtxList.nCount, m_chunks.size());
  p3DRenderDevice->DrawText(0, 10, 100, SmtColor(0., 1., 1.), szBuf);

  p3DRenderDevice->MatrixPop();
  return SMT_ERR_NONE;
}

long Smt3DPointCloud::Destroy() {
  SMT_SAFE_DELETE(m_pVertexBuffer);
  m_chunks.clear();
  m_point_index.DestroyTree();
  m_nLastDrawnPoints = 0;
  return SMT_ERR_NONE;
}

bool Smt3DPointCloud::Read3DPointCloud(const char* szFilePath) {
  ifstream infile;

  locale loc = locale::global(locale(".936"));
  infile.open(szFilePath, ios::in);
  locale::global(std::locale(loc));

  if (!infile.is_open()) {
    return false;
  }

  char szBuf[255];
  vSmtVertex3Ds vVtxs;

  while (!infile.eof()) {
    infile.getline(szBuf, 255, '\n');
    SmtVertex3D pcVer;
    Vector3 ver;
    int r, g, b;
    if (sscanf(szBuf, "%f,%f,%f,%d,%d,%d", &ver.x, &ver.z, &ver.y, &r, &g,
               &b) != 6) {
      continue;
    }

    pcVer.ver = ver * 10;
    pcVer.clr.fRed = r / 255.;
    pcVer.clr.fGreen = g / 255.;
    pcVer.clr.fBlue = b / 255.;

    vVtxs.push_back(pcVer);
  }

  infile.close();

  m_vtxList = vVtxs;
  m_bReadOK = true;
  return true;
}

}  // namespace render
