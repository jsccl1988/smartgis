// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/primitive/surface/pointcloud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <locale>
#include <unordered_map>
#include <vector>

#include "scenic/render/rhi3d/public/device/base.h"
#include "scenic/render/rhi3d/public/state/states_manager.h"

namespace scenic {
namespace detail {
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

PointCloud3d::PointCloud3d() = default;

PointCloud3d::~PointCloud3d() { Destroy(); }

long PointCloud3d::Init(::base::Vector3& vPos, Material& matMaterial,
                           const char* szTexName) {
  return Object3d::Init(vPos, matMaterial, szTexName);
}

void PointCloud3d::pack_vertices_for_chunks(Vertex3dList* packed) {
  if (!packed || vtx_list_.nCount < 1 || !vtx_list_.pVertexs) {
    return;
  }

  if (vtx_list_.nCount < kChunkPointThreshold) {
    *packed = vtx_list_;
    return;
  }

  Aabb bounds;
  for (int i = 0; i < vtx_list_.nCount; ++i) {
    bounds.merge(vtx_list_.pVertexs[i].ver);
  }
  const double ex =
      (std::max)(1.0e-6, static_cast<double>(bounds.vcMax.x - bounds.vcMin.x));
  const double ey =
      (std::max)(1.0e-6, static_cast<double>(bounds.vcMax.y - bounds.vcMin.y));
  const double ez =
      (std::max)(1.0e-6, static_cast<double>(bounds.vcMax.z - bounds.vcMin.z));

  int cells = static_cast<int>(
      std::cbrt(static_cast<double>(vtx_list_.nCount) / kTargetChunkPoints));
  cells = (std::max)(1, (std::min)(cells, 64));

  // unordered_map: O(n) expected vs tree map log factor on large clouds.
  std::unordered_map<ulong, std::vector<int>> buckets;
  buckets.reserve(static_cast<size_t>(cells * cells * cells / 2 + 1));
  for (int i = 0; i < vtx_list_.nCount; ++i) {
    const Vector3& v = vtx_list_.pVertexs[i].ver;
    int ix = static_cast<int>((v.x - bounds.vcMin.x) / ex * cells);
    int iy = static_cast<int>((v.y - bounds.vcMin.y) / ey * cells);
    int iz = static_cast<int>((v.z - bounds.vcMin.z) / ez * cells);
    ix = (std::max)(0, (std::min)(ix, cells - 1));
    iy = (std::max)(0, (std::min)(iy, cells - 1));
    iz = (std::max)(0, (std::min)(iz, cells - 1));
    buckets[cell_key(ix, iy, iz)].push_back(i);
  }

  // Stable cell order so chunk rebuild walks contiguous ranges.
  std::vector<ulong> keys;
  keys.reserve(buckets.size());
  for (const auto& entry : buckets) {
    keys.push_back(entry.first);
  }
  std::sort(keys.begin(), keys.end());

  vSmtVertex3Ds ordered;
  ordered.reserve(static_cast<size_t>(vtx_list_.nCount));
  for (ulong key : keys) {
    for (int idx : buckets[key]) {
      ordered.push_back(vtx_list_.pVertexs[idx]);
    }
  }
  *packed = ordered;
}

void PointCloud3d::build_chunks(const Vertex3dList& packed) {
  chunks_.clear();
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
    chunk.aabb.vcCenter = (chunk.aabb.vcMax + chunk.aabb.vcMin) * 0.5f;
    chunks_.push_back(chunk);
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
      cur.aabb.vcCenter = (cur.aabb.vcMax + cur.aabb.vcMin) * 0.5f;
      chunks_.push_back(cur);
      cur = PointCloudChunk{};
      cur.start = static_cast<ulong>(i);
      cur.count = 0;
      prev_key = key;
    }
    cur.aabb.merge(packed.pVertexs[i].ver);
    ++cur.count;
  }
  cur.aabb.vcCenter = (cur.aabb.vcMax + cur.aabb.vcMin) * 0.5f;
  chunks_.push_back(cur);
}

long PointCloud3d::build_gpu_buffer(LP3DRENDERDEVICE device) {
  Vertex3dList packed;
  pack_vertices_for_chunks(&packed);
  if (packed.nCount < 1 || !packed.pVertexs) {
    return SMT_ERR_FAILURE;
  }

  // Keep CPU list in draw order so unibn indices match VB order.
  vtx_list_ = packed;

  const int n = vtx_list_.nCount;
  std::vector<float> xyz(static_cast<size_t>(n) * 3);
  std::vector<float> rgba(static_cast<size_t>(n) * 4);
  for (int i = 0; i < n; ++i) {
    const Vertex3d& v = vtx_list_.pVertexs[i];
    const size_t o3 = static_cast<size_t>(i) * 3;
    const size_t o4 = static_cast<size_t>(i) * 4;
    xyz[o3] = v.ver.x;
    xyz[o3 + 1] = v.ver.y;
    xyz[o3 + 2] = v.ver.z;
    rgba[o4] = v.clr.fRed;
    rgba[o4 + 1] = v.clr.fGreen;
    rgba[o4 + 2] = v.clr.fBlue;
    rgba[o4 + 3] = v.clr.fA;
  }

  if (!upload_points(device, xyz.data(), static_cast<size_t>(n),
                     rgba.data())) {
    return SMT_ERR_FAILURE;
  }
  build_chunks(vtx_list_);
  return SMT_ERR_NONE;
}

long PointCloud3d::Create(LP3DRENDERDEVICE device) {
  if (!device) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!read_ok_ || vtx_list_.nCount < 1 || !vtx_list_.pVertexs) {
    return SMT_ERR_FAILURE;
  }

  const long gpu_err = build_gpu_buffer(device);
  if (gpu_err != SMT_ERR_NONE) {
    return gpu_err;
  }
  return point_index_.build(vtx_list_);
}

long PointCloud3d::Update(LP3DRENDERDEVICE /*device*/, float /*elapsed*/) {
  return SMT_ERR_NONE;
}

long PointCloud3d::Render(LP3DRENDERDEVICE device) {
  if (!device) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!vb_ || chunks_.empty()) {
    return SMT_ERR_NONE;
  }

  device->SetMaterial(&m_matMaterial);
  device->MatrixPush();
  device->MatrixMultiply(m_mtxModel);

  Frustum frustum;
  device->GetFrustum(frustum);

  if (show_bounds_) {
    if (GpuStateManager* states = device->GetStateManager()) {
      states->SetLight(false);
      states->Set2DTextures(false);
      const float width = static_cast<float>(
          (std::max)(m_aAbb.vcMax.x - m_aAbb.vcMin.x,
                     (std::max)(m_aAbb.vcMax.y - m_aAbb.vcMin.y,
                                m_aAbb.vcMax.z - m_aAbb.vcMin.z)));
      device->DrawCube3D(m_aAbb.vcCenter, width, Color(0.f, 1.f, 0.f, 1.f));
      states->SetLight(true);
      states->Set2DTextures(true);
    }
  }

  last_drawn_points_ = 0;
  for (const PointCloudChunk& chunk : chunks_) {
    if (!frustum.intersects(chunk.aabb)) {
      continue;
    }
    device->DrawPrimitives(PT_POINTLIST, vb_, chunk.start, chunk.count);
    last_drawn_points_ += static_cast<int>(chunk.count);
  }

  char buf[TEMP_BUFFER_SIZE];
  std::snprintf(buf, TEMP_BUFFER_SIZE, "points drawn:%d/%d chunks:%zu",
                last_drawn_points_, vtx_list_.nCount, chunks_.size());
  device->DrawText(0, 10, 100, Color(0.f, 1.f, 1.f), buf);

  device->MatrixPop();
  return SMT_ERR_NONE;
}

long PointCloud3d::Destroy() {
  release_gpu_buffers();
  chunks_.clear();
  point_index_.DestroyTree();
  last_drawn_points_ = 0;
  return SMT_ERR_NONE;
}

bool PointCloud3d::Read3DPointCloud(const char* path) {
  if (!path || !path[0]) {
    return false;
  }

  std::ifstream infile;
  const std::locale loc = std::locale::global(std::locale(".936"));
  infile.open(path, std::ios::in);
  std::locale::global(loc);

  if (!infile.is_open()) {
    return false;
  }

  char line[255];
  vSmtVertex3Ds vtxs;
  while (infile.getline(line, sizeof(line))) {
    Vertex3d pc;
    Vector3 ver;
    int r = 0;
    int g = 0;
    int b = 0;
    if (std::sscanf(line, "%f,%f,%f,%d,%d,%d", &ver.x, &ver.z, &ver.y, &r, &g,
                    &b) != 6) {
      continue;
    }
    pc.ver = ver * 10.f;
    pc.clr.fRed = r / 255.f;
    pc.clr.fGreen = g / 255.f;
    pc.clr.fBlue = b / 255.f;
    pc.clr.fA = 1.f;
    vtxs.push_back(pc);
  }

  vtx_list_ = vtxs;
  read_ok_ = vtx_list_.nCount > 0;
  return read_ok_;
}

}  // namespace detail
}  // namespace scenic
