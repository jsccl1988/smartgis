// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MD3D_POINTCLOUD_H
#define _MD3D_POINTCLOUD_H

#include <vector>

#include "legacy/core/macros/macros.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_device.h"
#include "legacy/render/rhi3d/public/resource/video_buffer.h"
#include "legacy/render/scene3d/index/vertex_octree.h"
#include "legacy/render/scene3d/scene/object.h"

using namespace render;

namespace render {

// One contiguous VB range for frustum-culled point drawing (P2).
struct PointCloudChunk {
  Aabb aabb;
  ulong start;
  ulong count;
};

// Leftover point cloud: owns VB (+ optional spatial chunks); unibn index is
// query-only via SmtVertexOctTree.
class LEGACY_RENDER_EXPORT Smt3DPointCloud : public Smt3DObject {
 public:
  Smt3DPointCloud();
  virtual ~Smt3DPointCloud();

 public:
  long Init(::base::Vector3& vPos, SmtMaterial& matMaterial);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Destroy();

 public:
  inline bool GetShowOctNodeBox(void) { return m_bShowBounds; }
  inline void SetShowOctNodeBox(bool bShow = true) { m_bShowBounds = bShow; }
  inline bool show_bounds() const { return m_bShowBounds; }
  inline void set_show_bounds(bool show) { m_bShowBounds = show; }

 public:
  bool Read3DPointCloud(const char* szFilePath);

  inline SmtVertexOctTree& GetVertexOctTree(void) { return m_point_index; }
  inline SmtVertexOctTree& point_index() { return m_point_index; }
  inline const SmtVertexOctTree& point_index() const { return m_point_index; }

 private:
  long build_gpu_buffer(LP3DRENDERDEVICE p3DRenderDevice);
  void build_chunks(const SmtVertex3DList& packed);
  void pack_vertices_for_chunks(SmtVertex3DList* packed);

 private:
  SmtVertexOctTree m_point_index;
  SmtVertexBuffer* m_pVertexBuffer;
  std::vector<PointCloudChunk> m_chunks;

  SmtVertex3DList m_vtxList;
  bool m_bShowBounds;
  bool m_bReadOK;
  int m_nLastDrawnPoints;
};

}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_MD3D_POINTCLOUD_H
